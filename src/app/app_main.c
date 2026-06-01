#include "app/app_main.h"

#include "app/app_state.h"
#include "network/tcp_client.h"
#include "network/packet.h"
#include "common/log.h"
#include "bsp/door.h"
#include "bsp/alarm.h"
#include "config/device_config.h"
#include "storage/snapshot_storage.h"
#include "camera/camera.h"

#include <stdio.h>
#include <string.h>


static void print_usage(const char *prog)
{
    printf("Usage:\n");
    printf("  %s <config_path>\n", prog);
    printf("\nExample:\n");
    printf("  %s ./config/device_config.json\n", prog);
}

/**
 * @brief 处理服务端返回的门禁识别结果（核心业务逻辑）
 * @param json_result 服务端返回的JSON字符串
 * @note 根据识别结果控制硬件：允许开门/拒绝报警/无人脸不动作
 */
static void app_handle_access_result(const char *json_result)
{
    LOG_INFO("server result raw: %s", json_result);

    if (strstr(json_result, "\"result\":\"allow\"") != NULL ||
        strstr(json_result, "\"result\": \"allow\"") != NULL) {

        LOG_INFO("识别结果: 允许通行");
        LOG_INFO("执行动作: door_open(3000)");

        app_state_set_result_allow("DengYangjie", 0.96f, "open_door", "face ok");
        door_open(3000);

    } else if (strstr(json_result, "\"result\":\"deny\"") != NULL ||
               strstr(json_result, "\"result\": \"deny\"") != NULL) {

        LOG_INFO("识别结果: 拒绝通行");
        LOG_INFO("执行动作: alarm_beep(1000)");

        app_state_set_result_deny("Unknown", 0.3f, "alarm_beep", "access denied");

        alarm_beep(1000);

    } else if (strstr(json_result, "\"result\":\"no_face\"") != NULL ||
               strstr(json_result, "\"result\": \"no_face\"") != NULL) {

        LOG_INFO("识别结果: 未检测到人脸");
        LOG_INFO("执行动作: none");

        app_state_set_result_no_face("no face detected");

    } else {
        LOG_ERROR("未知识别结果: %s", json_result);
        app_state_set_error("unknown result");
    }
}


static int app_init(const DeviceConfig *config)
{
    log_set_level(log_level_from_string(config->log_level));
    log_set_show_detail(config->log_detail);

    app_state_init();
    app_state_set_wifi_ok(1);
    app_state_set_tcp_online(0);
    app_state_set_status(APP_STATUS_IDLE);

    if (door_init() < 0) {
        LOG_ERROR("door init failed");
        return -1;
    }

    if (alarm_init() < 0) {
        LOG_ERROR("alarm init failed");
        return -1;
    }

    if (snapshot_storage_init(config->snapshot_dir) < 0) {
        LOG_ERROR("snapshot storage init failed");
        return -1;
    }

    if (camera_init(config->camera_dev, config->image_width, config->image_height) < 0) {
        LOG_ERROR("camera init failed");
        return -1;
    }



    LOG_INFO("app init success");
    return 0;
}


static void app_deinit(void)
{
    camera_deinit();

    door_deinit();
    alarm_deinit();
    
    LOG_INFO("app deinit done");
}

/**
 * @brief 执行一次完整的门禁识别流程
 * @param config 设备配置结构体指针（服务器IP、端口、设备ID等）
 * @return 成功返回0，失败返回-1
 * @note 流程：抓拍图片 -> 上传服务器 -> 接收结果 -> 控制硬件
 */
static int app_run_once(const DeviceConfig *config)
{
    // 1. 定义一个数组，用来存放【抓拍图片的保存路径】
    char snapshot_path[SNAPSHOT_PATH_MAX_LEN];

    // 2. 生成一个带时间戳的图片路径，比如：
    // ./output/snapshots/snap_20250524_153025.jpg
    if (snapshot_storage_make_path(snapshot_path, sizeof(snapshot_path)) < 0) {
        LOG_ERROR("make snapshot path failed");
        return -1;
    }

    app_state_set_status(APP_STATUS_CAPTURING);

    LOG_INFO("snapshot path: %s", snapshot_path);

    //摄像头抓拍一张jpg，保存到上面路径
    if (camera_capture_jpeg(snapshot_path) < 0) {
        LOG_ERROR("camera capture failed");
        app_state_set_error("camera capture failed");
        return -1;
    }


    LOG_INFO("server ip   : %s", config->server_ip);
    LOG_INFO("server port : %d", config->server_port);
    LOG_INFO("send image  : %s", snapshot_path);

    int sockfd = tcp_client_connect(config->server_ip, config->server_port);
    if (sockfd < 0) {
        LOG_ERROR("connect server failed");
        app_state_set_tcp_online(0);
        app_state_set_error("connect server failed");
        return -1;
    }

    app_state_set_tcp_online(1);
    LOG_INFO("connect server success");

    app_state_set_status(APP_STATUS_UPLOADING);

    if (packet_send_image_ex(sockfd, snapshot_path, config->device_id, "RGB565", config->image_width, config->image_height) < 0) {
        LOG_ERROR("send image failed");
        app_state_set_error("send image failed");
        tcp_client_close(sockfd);
        app_state_set_tcp_online(0);
        return -1;
    }

    app_state_set_status(APP_STATUS_VERIFYING);

    LOG_INFO("send image success, waiting for result...");

    char result_buf[1024];

    int n = tcp_recv_line(sockfd, result_buf, sizeof(result_buf));
    if (n <= 0) {
        LOG_ERROR("recv result failed");
        app_state_set_error("recv result failed");
        tcp_client_close(sockfd);
        app_state_set_tcp_online(0);
        return -1;
    }

    if (n >= (int)sizeof(result_buf)) {
    n = sizeof(result_buf) - 1;
    }
    result_buf[n] = '\0';

    app_handle_access_result(result_buf);

    tcp_client_close(sockfd);
    app_state_set_tcp_online(0);

    return 0;
}


int app_main(int argc, char *argv[])
{
    if (argc != 2) {
        print_usage(argv[0]);
        return -1;
    }

    //获取配置文件路径
    const char *config_path = argv[1];

    /*
     * 临时使用 INFO + 简洁模式，保证配置加载前也能打印错误。
     * 加载配置后会根据 config 重新设置日志等级。
     */
    log_set_level(LOG_LEVEL_INFO);
    log_set_show_detail(0);

    //定义设备配置结构体
    DeviceConfig config;

    // 从JSON文件加载设备配置
    if (device_config_load(config_path, &config) < 0) {
        LOG_ERROR("load config failed: %s", config_path);
        return -1;
    }

    if (app_init(&config) < 0) {
        LOG_ERROR("app init failed");
        return -1;
    }

    device_config_print(&config);

    int ret = app_run_once(&config);

    app_deinit();

    if (ret < 0) {
        LOG_ERROR("app run failed");
        return -1;
    }

    LOG_INFO("app exit");
    return 0;
}
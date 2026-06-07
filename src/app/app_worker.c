#include "app/app_worker.h"
#include "app/app_state.h"

#include "network/tcp_client.h"
#include "network/packet.h"
#include "common/log.h"
#include "bsp/door.h"
#include "bsp/alarm.h"
#include "storage/snapshot_storage.h"
#include "camera/camera.h"

#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

static pthread_t g_worker_thread;
static int g_worker_running = 0;
static DeviceConfig g_worker_config;

/**
 * @brief       解析云端返回识别JSON，控制门锁与蜂鸣外设
 * @param[in]   json_result 云端原始结果字符串
 * @retval      无
 */
static void app_handle_access_result(const char *json_result)
{
    LOG_INFO("server result raw: %s", json_result);

    /* 放行：云端返回allow结果 */
    if (strstr(json_result, "\"result\":\"allow\"") != NULL ||
        strstr(json_result, "\"result\": \"allow\"") != NULL) {

        LOG_INFO("识别结果: 允许通行");
        app_state_set_result_allow("DengYangjie", 0.96f, "open_door", "face ok");

        LOG_INFO("执行动作: door_open(3000)");
        door_open(3000);

    }
    /* 拒绝通行：云端返回deny结果 */
    else if (strstr(json_result, "\"result\":\"deny\"") != NULL ||
             strstr(json_result, "\"result\": \"deny\"") != NULL) {

        LOG_INFO("识别结果: 拒绝通行");
        app_state_set_result_deny("Unknown", 0.30f, "alarm_beep", "access denied");

        LOG_INFO("执行动作: alarm_beep(1000)");
        alarm_beep(1000);

    }
    /* 未检测到人脸 */
    else if (strstr(json_result, "\"result\":\"no_face\"") != NULL ||
             strstr(json_result, "\"result\": \"no_face\"") != NULL) {

        LOG_INFO("识别结果: 未检测到人脸");
        LOG_INFO("执行动作: none");
        app_state_set_result_no_face("no face detected");

    }
    /* 未知返回字段，异常记录 */
    else {
        LOG_ERROR("未知识别结果: %s", json_result);
        app_state_set_error("unknown result");
    }
}

/**
 * @brief       单次完整业务：抓拍-上传-收结果
 * @param[in]   config 设备配置结构体指针
 * @retval      0:成功  -1:流程异常失败
 */

static int app_worker_run_once(const DeviceConfig *config)
{
    char snapshot_path[SNAPSHOT_PATH_MAX_LEN];

    /* 拼接图片本地存储路径 */
    if (snapshot_storage_make_path(snapshot_path, sizeof(snapshot_path)) < 0) {
        LOG_ERROR("make snapshot path failed");
        app_state_set_error("make snapshot path failed");
        return -1;
    }
    app_state_set_status(APP_STATUS_CAPTURING);
    LOG_INFO("snapshot path: %s", snapshot_path);

    /* 摄像头采集图像，接口名JPEG实际存储RGB565裸数据 */
    if (camera_capture_jpeg(snapshot_path) < 0) {
        LOG_ERROR("camera capture failed");
        app_state_set_error("camera capture failed");
        return -1;
    }

    LOG_INFO("server ip   : %s", config->server_ip);
    LOG_INFO("server port : %d", config->server_port);
    LOG_INFO("send image  : %s", snapshot_path);

    /* 建立TCP短连接 */
    int sockfd = tcp_client_connect(config->server_ip, config->server_port);
    if (sockfd < 0) {
        LOG_ERROR("connect server failed");
        app_state_set_tcp_online(0);
        app_state_set_error("connect server failed");
        return -1;
    }
    app_state_set_tcp_online(1);
    app_state_set_status(APP_STATUS_UPLOADING);

    /* 封装协议上传RGB565图像文件 */
    if (packet_send_image_ex(sockfd,
                             snapshot_path,
                             config->device_id,
                             "RGB565",
                             config->image_width,
                             config->image_height) < 0) {
        LOG_ERROR("send image failed");
        app_state_set_error("send image failed");
        tcp_client_close(sockfd);
        app_state_set_tcp_online(0);
        return -1;
    }

    app_state_set_status(APP_STATUS_VERIFYING);
    LOG_INFO("send image success, waiting for result...");

    char result_buf[1024];
    /* 阻塞读取云端返回一行JSON数据 */
    int n = tcp_recv_line(sockfd, result_buf, sizeof(result_buf));
    if (n <= 0) {
        LOG_ERROR("recv result failed");
        app_state_set_error("recv result failed");
        tcp_client_close(sockfd);
        app_state_set_tcp_online(0);
        return -1;
    }

    /* 防止缓冲区越界，截断字符串 */
    if (n >= (int)sizeof(result_buf))
        n = sizeof(result_buf) - 1;
    result_buf[n] = '\0';

    app_handle_access_result(result_buf);

    /* 单次任务结束，关闭套接字 */
    tcp_client_close(sockfd);
    app_state_set_tcp_online(0);

    return 0;
}

/**
 * @brief       工作线程入口，循环定时执行识别任务
 * @param[in]   arg 设备配置参数地址
 * @retval      NULL
 */
static void *app_worker_thread_func(void *arg)
{
    DeviceConfig *config = (DeviceConfig *)arg;
    LOG_INFO("[WORKER] thread started");

    /* 主循环，标记置0退出循环 */
    while (g_worker_running) {
        int ret = app_worker_run_once(config);
        if (ret < 0)
            LOG_ERROR("[WORKER] run once failed");
        else
            LOG_INFO("[WORKER] run once success");

        /* 5秒周期延时，分段sleep可快速响应停止指令 */
        for (int i = 0; i < 5; i++) {
            if (!g_worker_running) break;
            sleep(1);
        }
    }

    LOG_INFO("[WORKER] thread stopped");
    return NULL;
}

/**
 * @brief       创建并启动识别工作线程
 * @param[in]   config 外部传入设备配置
 * @retval      0:启动成功  -1:入参非法/线程创建失败
 */
int app_worker_start(const DeviceConfig *config)
{
    if (!config) {
        LOG_ERROR("[WORKER] invalid config");
        return -1;
    }
    /* 重复启动直接返回 */
    if (g_worker_running) {
        LOG_WARN("[WORKER] already running");
        return 0;
    }

    /* 拷贝配置至全局，隔离外部指针 */
    memcpy(&g_worker_config, config, sizeof(DeviceConfig));
    g_worker_running = 1;

    if (pthread_create(&g_worker_thread, NULL, app_worker_thread_func, &g_worker_config) != 0) {
        LOG_ERROR("[WORKER] pthread_create failed");
        g_worker_running = 0;
        return -1;
    }
    return 0;
}

/**
 * @brief       停止工作线程，等待线程安全退出
 * @retval      无
 */
void app_worker_stop(void)
{
    if (!g_worker_running) return;
    g_worker_running = 0;
    /* 阻塞等待子线程结束，资源正常释放 */
    pthread_join(g_worker_thread, NULL);
}

/**
 * @brief       查询工作线程运行标志
 * @retval      1:运行中  0:已停止
 */
int app_worker_is_running(void)
{
    return g_worker_running;
}

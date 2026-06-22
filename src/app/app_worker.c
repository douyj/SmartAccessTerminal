#include "app/app_worker.h"
#include "app/app_state.h"

#include "network/tcp_client.h"
#include "network/packet.h"
#include "common/log.h"
#include "bsp/door.h"
#include "bsp/alarm.h"
#include "storage/snapshot_storage.h"
#include "camera/camera.h"
#include "camera/camera_preview.h"
#include "app/app_trigger.h"

#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

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

static int save_rgb565_file(const char *path, const unsigned char *data, int size)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        LOG_ERROR("[WORKER] fopen failed: %s", path);
        return -1;
    }

    int written = fwrite(data, 1, size, fp);
    fclose(fp);

    if (written != size) {
        LOG_ERROR("[WORKER] fwrite failed, written=%d, expected=%d", written, size);
        return -1;
    }

    return 0;
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

static int app_worker_run_once_from_preview(const DeviceConfig *config)
{
    char snapshot_path[SNAPSHOT_PATH_MAX_LEN];

    if (snapshot_storage_make_path(snapshot_path, sizeof(snapshot_path)) < 0) {
        LOG_ERROR("make snapshot path failed");
        app_state_set_error("make snapshot path failed");
        return -1;
    }

    int w = 0;
    int h = 0;
    int frame_size = 0;

    camera_preview_get_info(&w, &h, &frame_size);

    if (w <= 0 || h <= 0 || frame_size <= 0) {
        LOG_ERROR("[WORKER] invalid preview frame info: %dx%d size=%d", w, h, frame_size);
        app_state_set_error("invalid preview frame");
        return -1;
    }

    unsigned char *frame_buf = (unsigned char *)malloc(frame_size);
    if (!frame_buf) {
        LOG_ERROR("[WORKER] malloc frame_buf failed");
        app_state_set_error("malloc frame failed");
        return -1;
    }

    app_state_set_status(APP_STATUS_CAPTURING);

    if (camera_preview_get_frame(frame_buf, frame_size, &w, &h, &frame_size) < 0) {
        LOG_ERROR("[WORKER] get preview frame failed");
        app_state_set_error("get preview frame failed");
        free(frame_buf);
        return -1;
    }

    LOG_INFO("[WORKER] got preview frame: %dx%d, size=%d", w, h, frame_size);
    LOG_INFO("[WORKER] snapshot path: %s", snapshot_path);

    if (save_rgb565_file(snapshot_path, frame_buf, frame_size) < 0) {
        LOG_ERROR("[WORKER] save preview frame failed");
        app_state_set_error("save preview frame failed");
        free(frame_buf);
        return -1;
    }

    free(frame_buf);
    frame_buf = NULL;

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

    /*
     * 注意：这里上传的宽高必须使用 preview 实际宽高 w/h，
     * 不能再固定使用 config->image_width / image_height。
     */
    if (packet_send_image_ex(sockfd,
                             snapshot_path,
                             config->device_id,
                             "RGB565",
                             w,
                             h) < 0) {
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

static void *app_worker_preview_thread_func(void *arg)
{
    DeviceConfig *config = (DeviceConfig *)arg;

    LOG_INFO("[WORKER] preview trigger worker thread started");
    LOG_INFO("[WORKER] waiting for trigger...");

    while (g_worker_running) {
        /*
         * 等待触发信号。
         * 返回值：
         *  1：收到触发，执行一次识别
         *  0：超时，继续等待
         * -1：停止
         */
        int trig = app_trigger_wait(1000);

        if (!g_worker_running) {
            break;
        }

        if (trig < 0) {
            break;
        }

        if (trig == 0) {
            continue;
        }

        LOG_INFO("[WORKER] trigger received, run recognition once");

        /*
         * 关键：开始识别前标记 BUSY。
         * 此时再按回车会被 app_trigger_request() 拒绝。
         */
        app_trigger_mark_busy();

        int ret = app_worker_run_once_from_preview(config);

        if (ret < 0) {
            LOG_ERROR("[WORKER] preview triggered run failed");
        } else {
            LOG_INFO("[WORKER] preview triggered run success");
        }

        /*
         * 关键：识别完成后进入 COOLDOWN。
         * 冷却期间再次触发会被忽略。
         */
        app_trigger_mark_done();

        LOG_INFO("[WORKER] waiting for next trigger...");
    }

    LOG_INFO("[WORKER] preview trigger worker thread stopped");
    return NULL;
}

int app_worker_start_from_preview(const DeviceConfig *config)
{
    if (!config) {
        LOG_ERROR("[WORKER] invalid config");
        return -1;
    }

    if (g_worker_running) {
        LOG_WARN("[WORKER] already running");
        return 0;
    }

    memcpy(&g_worker_config, config, sizeof(DeviceConfig));

    g_worker_running = 1;

    if (pthread_create(&g_worker_thread,
                       NULL,
                       app_worker_preview_thread_func,
                       &g_worker_config) != 0) {
        LOG_ERROR("[WORKER] pthread_create preview failed");
        g_worker_running = 0;
        return -1;
    }

    return 0;
}
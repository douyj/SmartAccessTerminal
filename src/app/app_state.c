/**
 * @file    app/app_state.c
 * @brief   应用全局状态管理实现文件
 * @details 基于互斥锁实现多线程安全的状态读写，封装状态初始化、状态更新、
 *          识别结果设置、状态快照获取等功能，用于业务、网络、UI 线程间数据同步
 * @author  Deng Yangjie
 * @date    2026-05-31
 * @version V1.0
 * @note    所有状态操作均加线程互斥保护，避免多线程并发访问引发数据错乱
 */
#include "app/app_state.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

static AppStateSnapshot g_state;
static pthread_mutex_t g_state_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief       安全字符串拷贝函数（防止缓冲区溢出、空指针）
 * @param[out]  dst         目标字符缓冲区
 * @param[in]   dst_size    目标缓冲区最大长度
 * @param[in]   src         源字符串
 * @retval      无
 */
static void safe_copy(char *dst, int dst_size, const char *src)
{
    if (dst == NULL || dst_size <= 0) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    snprintf(dst, dst_size, "%s", src);
}

/**
 * @brief       初始化应用状态模块
 * @retval      无
 * @note        清空全局状态并设置默认初始值，WiFi/TCP 默认断开，状态置为空闲
 */
void app_state_init(void)
{
    pthread_mutex_lock(&g_state_mutex);

    memset(&g_state, 0, sizeof(g_state));

    g_state.wifi_ok = 0;
    g_state.tcp_online = 0;

    g_state.status = APP_STATUS_IDLE;
    g_state.result = APP_RESULT_NONE;

    safe_copy(g_state.name, sizeof(g_state.name), "");
    g_state.confidence = 0.0f;
    safe_copy(g_state.action, sizeof(g_state.action), "");
    safe_copy(g_state.message, sizeof(g_state.message), "System ready");

    /* 清除结果更新标记 */
    g_state.result_updated = 0;

    pthread_mutex_unlock(&g_state_mutex);
}

/**
 * @brief       设置WiFi连接状态
 * @param[in]   ok  1-正常可用  0-异常/断开
 * @retval      无
 */
void app_state_set_wifi_ok(int ok)
{
    pthread_mutex_lock(&g_state_mutex);
    g_state.wifi_ok = ok ? 1 : 0;
    pthread_mutex_unlock(&g_state_mutex);
}

/**
 * @brief       设置TCP网络连接状态
 * @param[in]   online  1-在线已连接  0-离线断开
 * @retval      无
 */
void app_state_set_tcp_online(int online)
{
    pthread_mutex_lock(&g_state_mutex);
    g_state.tcp_online = online ? 1 : 0;
    pthread_mutex_unlock(&g_state_mutex);
}

/**
 * @brief       设置应用当前运行状态
 * @param[in]   status  应用状态枚举 @ref AppStatus
 * @retval      无
 */
void app_state_set_status(AppStatus status)
{
    pthread_mutex_lock(&g_state_mutex);
    g_state.status = status;
    pthread_mutex_unlock(&g_state_mutex);
}

/**
 * @brief       设置人脸识别通过结果
 * @param[in]   name        识别人员姓名
 * @param[in]   confidence  识别置信度
 * @param[in]   action      设备动作描述
 * @param[in]   message     状态提示信息
 * @retval      无
 * @note        自动置位 result_updated 标记，通知UI刷新界面
 */
void app_state_set_result_allow(const char *name,
                                float confidence,
                                const char *action,
                                const char *message)
{
    pthread_mutex_lock(&g_state_mutex);

    g_state.status = APP_STATUS_SUCCESS;
    g_state.result = APP_RESULT_ALLOW;

    safe_copy(g_state.name, sizeof(g_state.name), name ? name : "Unknown");
    g_state.confidence = confidence;
    safe_copy(g_state.action, sizeof(g_state.action), action ? action : "open_door");
    safe_copy(g_state.message, sizeof(g_state.message), message ? message : "access granted");

    g_state.result_updated = 1;

    pthread_mutex_unlock(&g_state_mutex);
}

/**
 * @brief       设置人脸识别拒绝结果
 * @param[in]   name        识别人员姓名
 * @param[in]   confidence  识别置信度
 * @param[in]   action      设备动作描述
 * @param[in]   message     状态提示信息
 * @retval      无
 * @note        自动置位 result_updated 标记，通知UI刷新界面
 */
void app_state_set_result_deny(const char *name,
                               float confidence,
                               const char *action,
                               const char *message)
{
    pthread_mutex_lock(&g_state_mutex);

    g_state.status = APP_STATUS_DENY;
    g_state.result = APP_RESULT_DENY;

    safe_copy(g_state.name, sizeof(g_state.name), name ? name : "Unknown");
    g_state.confidence = confidence;
    safe_copy(g_state.action, sizeof(g_state.action), action ? action : "alarm_beep");
    safe_copy(g_state.message, sizeof(g_state.message), message ? message : "access denied");

    g_state.result_updated = 1;

    pthread_mutex_unlock(&g_state_mutex);
}

/**
 * @brief       设置未检测到人脸结果
 * @param[in]   message  提示信息
 * @retval      无
 * @note        自动置位 result_updated 标记，通知UI刷新界面
 */
void app_state_set_result_no_face(const char *message)
{
    pthread_mutex_lock(&g_state_mutex);

    g_state.status = APP_STATUS_NO_FACE;
    g_state.result = APP_RESULT_NO_FACE;

    safe_copy(g_state.name, sizeof(g_state.name), "None");
    g_state.confidence = 0.0f;
    safe_copy(g_state.action, sizeof(g_state.action), "none");
    safe_copy(g_state.message, sizeof(g_state.message), message ? message : "no face detected");

    g_state.result_updated = 1;

    pthread_mutex_unlock(&g_state_mutex);
}

/**
 * @brief       设置系统/网络错误状态
 * @param[in]   message  错误描述信息
 * @retval      无
 * @note        自动置位 result_updated 标记，通知UI刷新界面
 */
void app_state_set_error(const char *message)
{
    pthread_mutex_lock(&g_state_mutex);

    g_state.status = APP_STATUS_ERROR;
    g_state.result = APP_RESULT_ERROR;

    safe_copy(g_state.name, sizeof(g_state.name), "Error");
    g_state.confidence = 0.0f;
    safe_copy(g_state.action, sizeof(g_state.action), "none");
    safe_copy(g_state.message, sizeof(g_state.message), message ? message : "system error");

    g_state.result_updated = 1;

    pthread_mutex_unlock(&g_state_mutex);
}

/**
 * @brief       获取当前完整应用状态快照
 * @param[out]  out  接收状态数据的结构体指针
 * @retval      无
 * @note        入参为空则直接返回；拷贝全局状态至输出结构体，保证数据一致性
 */
void app_state_get_snapshot(AppStateSnapshot *out)
{
    if (out == NULL) {
        return;
    }

    pthread_mutex_lock(&g_state_mutex);
    memcpy(out, &g_state, sizeof(AppStateSnapshot));
    pthread_mutex_unlock(&g_state_mutex);
}


/**
 * @brief       清除结果更新标记
 * @retval      无
 * @note        UI线程读取完新状态后调用，避免重复处理同一结果
 */
void app_state_clear_result_updated(void)
{
    pthread_mutex_lock(&g_state_mutex);
    g_state.result_updated = 0;
    pthread_mutex_unlock(&g_state_mutex);
}

#ifndef APP_STATE_H
#define APP_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file    app_state.h
 * @brief   应用全局状态管理头文件
 * @details 定义应用状态枚举、状态快照结构体，提供状态读写、结果更新接口，
 *          用于业务层与UI层、网络层之间状态同步
 * @author  Deng Yangjie
 * @date    2026-05-31
 * @version V1.0
 * @note    多线程场景下建议配合互斥锁使用，保证数据读写安全
 */

 /* ===================== 长度限制宏定义 ===================== */
#define APP_STATE_NAME_MAX_LEN      64
#define APP_STATE_MSG_MAX_LEN       128
#define APP_STATE_ACTION_MAX_LEN    64


/**
 * @enum  AppStatus
 * @brief 应用整体运行状态枚举
 */
typedef enum {
    APP_STATUS_IDLE = 0,    /**< 空闲待机状态 */
    APP_STATUS_CAPTURING,
    APP_STATUS_UPLOADING,
    APP_STATUS_VERIFYING,
    APP_STATUS_SUCCESS,
    APP_STATUS_DENY,
    APP_STATUS_NO_FACE,
    APP_STATUS_ERROR
} AppStatus;

/**
 * @enum  AppResult
 * @brief 人脸识别最终结果枚举
 */
typedef enum {
    APP_RESULT_NONE = 0,
    APP_RESULT_ALLOW,
    APP_RESULT_DENY,
    APP_RESULT_NO_FACE,
    APP_RESULT_ERROR
} AppResult;

/**
 * @struct AppStateSnapshot
 * @brief 应用状态快照结构体
 * @note  统一汇总系统所有状态信息，供UI线程轮询读取；
 *        result_updated 为结果更新标记，UI读取后主动清零
 */
typedef struct {
    int wifi_ok;                            /**< WiFi状态：1-正常  0-异常 */
    int tcp_online;                         /**< TCP连接状态：1-在线  0-离线 */

    AppStatus status;                       /**< 当前应用运行状态 */
    AppResult result;                       /**< 人脸识别最终结果 */

    char name[APP_STATE_NAME_MAX_LEN];      /**< 识别到的人员姓名 */
    float confidence;                       /**< 人脸识别置信度 范围：0.0 ~ 1.0 */

    char action[APP_STATE_ACTION_MAX_LEN];  /**< 设备动作描述（如开门、报警等） */
    char message[APP_STATE_MSG_MAX_LEN];    /**< 状态提示/错误信息 */

    /**
     * @brief 识别结果更新标记
     * @note  业务层置1表示有新结果；UI层读取完成后调用接口清零
     */
    int result_updated;
} AppStateSnapshot;


void app_state_init(void);

void app_state_set_wifi_ok(int ok);
void app_state_set_tcp_online(int online);

void app_state_set_status(AppStatus status);

void app_state_set_result_allow(const char *name,
                                float confidence,
                                const char *action,
                                const char *message);

void app_state_set_result_deny(const char *name,
                               float confidence,
                               const char *action,
                               const char *message);

void app_state_set_result_no_face(const char *message);

void app_state_set_error(const char *message);

void app_state_get_snapshot(AppStateSnapshot *out);

void app_state_clear_result_updated(void);

#ifdef __cplusplus
}
#endif

#endif

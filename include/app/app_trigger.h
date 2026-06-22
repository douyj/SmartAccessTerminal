#ifndef APP_TRIGGER_H
#define APP_TRIGGER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_TRIGGER_IDLE = 0,
    APP_TRIGGER_BUSY,
    APP_TRIGGER_COOLDOWN,
    APP_TRIGGER_STOPPED
} AppTriggerState;

void app_trigger_init(void);

/*
 * 请求触发一次识别。
 * 返回值：
 *   1：触发请求成功
 *   0：被忽略，例如 busy/cooldown
 *  -1：触发系统已停止
 */
int app_trigger_request(void);

/*
 * worker 等待触发。
 * 返回值：
 *   1：收到触发，worker 应执行一次识别
 *   0：超时
 *  -1：停止
 */
int app_trigger_wait(int timeout_ms);

/*
 * worker 开始执行识别时调用。
 */
void app_trigger_mark_busy(void);

/*
 * worker 识别结束后调用，进入冷却状态。
 */
void app_trigger_mark_done(void);

/*
 * 周期调用，用于从 cooldown 自动恢复到 idle。
 */
void app_trigger_update(void);

void app_trigger_stop(void);

AppTriggerState app_trigger_get_state(void);

#ifdef __cplusplus
}
#endif

#endif

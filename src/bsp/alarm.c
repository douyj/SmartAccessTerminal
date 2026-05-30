#include "bsp/alarm.h"
#include "common/log.h"

#include <unistd.h>

/* 全局状态标志 */
static int g_alarm_inited = 0;  // 告警模块是否初始化
static int g_alarm_on = 0;       // 告警是否正在响

/**
 * @brief  告警模块初始化
 * @param  无
 * @return 0 成功
 * @note   防重复初始化
 */
int alarm_init(void)
{
    if (g_alarm_inited) {
        return 0;
    }

    /*
     * 模拟初始化：
     * 真实硬件：初始化 GPIO 为输出模式
     */
    LOG_INFO("[BSP-ALARM] init success");

    g_alarm_inited = 1;
    g_alarm_on = 0;

    return 0;
}

/**
 * @brief  启动蜂鸣/告警
 * @param  duration_ms  持续时间（毫秒）
 *                      0 = 只响不自动停
 *                      >0 = 定时后自动停止
 *                      <0 = 参数错误
 * @return 0 成功，-1 参数错误
 * @note   未初始化会自动初始化
 */
int alarm_beep(int duration_ms)
{
    if (!g_alarm_inited) {
        LOG_WARN("[BSP-ALARM] not initialized, auto init");
        alarm_init();
    }

    if (duration_ms < 0) {
        LOG_ERROR("[BSP-ALARM] invalid duration: %d ms", duration_ms);
        return -1;
    }

    /*
     * 模拟告警开启：
     * 真实硬件：gpio_write_value(ALARM_GPIO, 1)
     */
    g_alarm_on = 1;
    LOG_INFO("[BSP-ALARM] BEEP");

    /* 定时自动停止 */
    if (duration_ms > 0) {
        LOG_INFO("[BSP-ALARM] keep beep %d ms", duration_ms);
        usleep(duration_ms * 1000);  // 毫秒转微秒延时
        alarm_stop();
    }

    return 0;
}

/**
 * @brief  停止告警/蜂鸣
 * @param  无
 * @return 0 成功
 * @note   未初始化会自动初始化
 */
int alarm_stop(void)
{
    if (!g_alarm_inited) {
        LOG_WARN("[BSP-ALARM] not initialized, auto init");
        alarm_init();
    }

    /*
     * 模拟告警关闭：
     * 真实硬件：gpio_write_value(ALARM_GPIO, 0)
     */
    g_alarm_on = 0;
    LOG_INFO("[BSP-ALARM] STOP");

    return 0;
}

/**
 * @brief  反初始化（模块退出）
 * @param  无
 * @return 0 成功
 * @note   自动停止告警，确保安全状态
 */
int alarm_deinit(void)
{
    if (!g_alarm_inited) {
        return 0;
    }

    /* 退出前确保关闭告警 */
    if (g_alarm_on) {
        alarm_stop();
    }

    LOG_INFO("[BSP-ALARM] deinit");

    g_alarm_inited = 0;
    return 0;
}

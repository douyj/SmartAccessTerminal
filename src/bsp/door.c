#include "bsp/door.h"
#include "common/log.h"

#include <unistd.h>

/* 全局驱动状态标志 */
static int g_door_inited = 0;   /* 驱动是否已初始化 */
static int g_door_opened = 0;    /* 门锁当前是否打开 */

/**
 * @brief  门锁驱动初始化
 * @param  无
 * @return 0 成功
 * @note   仅初始化一次，重复调用直接返回成功
 */
int door_init(void)
{
    if(g_door_inited){
        return 0;
    }

    /*
     * 模拟初始化：
     * 真实硬件平台：此处初始化GPIO为输出模式
     */
    LOG_INFO("[BSP-DOOR] init success");

    g_door_inited = 1;
    g_door_opened = 0;

    return 0;
}

/**
 * @brief  打开门锁，并支持延时自动关门
 * @param  duration_ms  开门保持时间（毫秒）
 *                      0 表示只开门不自动关
 *                      >0 表示保持duration_ms毫秒后自动关门
 *                      <0 非法参数
 * @return 0 成功，-1 参数错误
 * @note   未初始化会自动初始化
 */
int door_open(int duration_ms)
{
    if(!g_door_inited){
        LOG_WARN("[BSP-DOOR] not initialized, auto init");
        door_init();
    }

    if(duration_ms < 0){
        LOG_ERROR("[BSP-DOOR] invalid duration: %d ms", duration_ms);
        return -1;
    }

    /*
     * 模拟开门动作：
     * 真实硬件：输出高电平触发继电器开门
     */
    g_door_opened = 1;
    LOG_INFO("[BSP-DOOR] OPEN");

    /* 如果设置了保持时间，则延时后自动关门 */
    if(duration_ms > 0){
        LOG_INFO("[BSP-DOOR] keep open %d ms", duration_ms);
        usleep(duration_ms * 1000);     //毫秒转微秒延时
        door_close();
    }

    return 0;
}

/**
 * @brief  主动关闭门锁
 * @param  无
 * @return 0 成功
 * @note   未初始化会自动初始化
 */
int door_close(void)
{
    if(!g_door_inited){
        LOG_WARN("[BSP-DOOR] not initialized, auto init");
        door_init();
    }

    /*
     * 模拟关门动作：
     * 真实硬件：输出低电平释放继电器
     */
    g_door_opened = 0;
    LOG_INFO("[BSP-DOOR] CLOSE");

    return 0;
}

/**
 * @brief  门锁驱动反初始化（退出时使用）
 * @param  无
 * @return 0 成功
 * @note   会自动关闭门锁，确保安全状态
 */
int door_deinit(void)
{
    if(!g_door_inited){
        return 0;
    }

    /*反初始化前确保门锁关闭*/
    if(g_door_opened){
        door_close();
    }

    g_door_inited = 0;
    return 0;
}
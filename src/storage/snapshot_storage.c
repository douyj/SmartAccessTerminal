/**
 * @file        snapshot_storage.c
 * @brief       图片快照存储管理模块
 * @details     负责创建快照保存目录、生成带时间戳的唯一文件名
 *              用于门禁系统抓拍图片的本地存储
 * @author      嵌入式存储模块
 * @date        2026
 */

#include "storage/snapshot_storage.h"
#include "common/log.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

/* 全局默认快照保存目录 */
static char g_snapshot_dir[SNAPSHOT_PATH_MAX_LEN] = "./output/snapshots";


/**
 * @brief  确保目录存在，不存在则创建
 * @param  dir  目标目录路径
 * @return 0 成功，-1 失败
 * @note   权限 0755：所有者可读写执行，其他只读执行
 */
/**
 * @brief  确保目录存在，不存在则创建
 * @param  dir  目标目录路径
 * @return 0 成功，-1 失败
 * @note   权限 0755：所有者可读写执行，其他只读执行
 */
static int ensure_dir_exists(const char *dir)
{
    struct stat st;

    if (dir == NULL || dir[0] == '\0') {
        return -1;
    }

    // 目录已存在
    if (stat(dir, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;
        }

        LOG_ERROR("path exists but is not directory: %s", dir);
        return -1;
    }

    // 创建目录
    if (mkdir(dir, 0755) != 0) {
        LOG_ERROR("mkdir failed: %s", dir);
        return -1;
    }

    return 0;
}

/**
 * @brief  生成用于文件名的时间字符串（格式：年月日_时分秒）
 * @param  buf   存储时间字符串的缓冲区
 * @param  size  缓冲区大小
 * @return 无
 */
static void get_time_string_for_file(char *buf, int size)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    if (tm_info == NULL) {
        snprintf(buf, size, "unknown_time");
        return;
    }

    // 格式化时间：20250524_153025
    strftime(buf, size, "%Y%m%d_%H%M%S", tm_info);
}

/**
 * @brief  初始化快照存储模块
 * @param  snapshot_dir  自定义快照目录（传 NULL 使用默认值）
 * @return 0 成功，-1 失败
 * @note   会自动创建目录
 */
int snapshot_storage_init(const char *snapshot_dir)
{
    // 如果传入了自定义目录，则覆盖默认目录
    if (snapshot_dir != NULL && snapshot_dir[0] != '\0') {
        snprintf(g_snapshot_dir, sizeof(g_snapshot_dir), "%s", snapshot_dir);
    }

    // 确保目录存在
    if (ensure_dir_exists(g_snapshot_dir) < 0) {
        LOG_ERROR("snapshot storage init failed");
        return -1;
    }

    LOG_INFO("[STORAGE] snapshot dir: %s", g_snapshot_dir);
    return 0;
}

/**
 * @brief  生成一条唯一的快照保存路径（带时间戳）
 * @param  path_buf  输出路径缓冲区
 * @param  buf_size  缓冲区大小
 * @return 0 成功，-1 失败
 * @note   路径格式：./output/snapshots/snap_20250524_153025.jpg
 */
int snapshot_storage_make_path(char *path_buf, int buf_size)
{
    if (path_buf == NULL || buf_size <= 0) {
        return -1;
    }

    char time_str[64];
    get_time_string_for_file(time_str, sizeof(time_str));

    // 拼接最终保存路径
    snprintf(
        path_buf,
        buf_size,
        "%s/snap_%s.jpg",
        g_snapshot_dir,
        time_str
    );

    return 0;
}

/**
 * @brief  获取当前快照存储目录
 * @return 目录路径字符串
 */
const char *snapshot_storage_get_dir(void)
{
    return g_snapshot_dir;
}
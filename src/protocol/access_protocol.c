#include "protocol/access_protocol.h"

#include <stdio.h>
#include <time.h>

/**
 * @brief 获取当前系统时间，并格式化为 "年-月-日 时:分:秒" 字符串
 * @param buf 存储时间字符串的缓冲区
 * @param size 缓冲区最大大小
 */

static void get_time_string(char *buf, int size)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    if(tm_info == NULL)
    {
        snprintf(buf, size, "unknow");
        return;
    }

    strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/**
 * @brief 构造图片上传的JSON协议头（给服务器识别用）
 * @param json_buf 存储JSON字符串的缓冲区
 * @param buf_size 缓冲区大小
 * @param device_id 设备ID
 * @param filename 图片文件名
 * @param image_size 图片大小（字节）
 * @return 生成的JSON字符串长度
 */
int access_build_snapshot_header(char *json_buf, int buf_size, const char *device_id, const char *filename, int image_size)
{
    char time_str[64];
    get_time_string(time_str,sizeof(time_str));

    return snprintf(
        json_buf,
        buf_size,
        "{"
        "\"type\":\"snapshot\","
        "\"device_id\":\"%s\","
        "\"filename\":\"%s\","
        "\"image_size\":%d,"
        "\"timestamp\":\"%s\""
        "}",
        device_id,
        filename,
        image_size,
        time_str
    );
}
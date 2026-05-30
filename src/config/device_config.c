#include "config/device_config.h"
#include "common/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/**
 * @brief  设置配置默认值
 * @param  config  设备配置结构体指针
 * @return 无
 * @note   未加载到配置文件时使用默认值
 */
static void config_set_default(DeviceConfig *config)
{
    memset(config, 0, sizeof(DeviceConfig));

    snprintf(config->device_id, sizeof(config->device_id), "imx6ull_001");

    snprintf(config->server_ip, sizeof(config->server_ip), "127.0.0.1");
    config->server_port = 9000;

    snprintf(config->camera_dev, sizeof(config->camera_dev), "/dev/video1");
    config->image_width = 640;
    config->image_height = 480;

    snprintf(config->test_image_path, sizeof(config->test_image_path), "./test_data/dyj.jpg");
    snprintf(config->snapshot_dir, sizeof(config->snapshot_dir), "./output/snapshots");

    snprintf(config->log_level, sizeof(config->log_level), "INFO");
    config->log_detail = 0;
}

/**
 * @brief  读取整个文件到内存字符串
 * @param  path  文件路径
 * @return 成功返回分配好的字符串指针，失败返回NULL
 * @note   调用者必须使用 free() 释放返回的内存
 */
static char *read_file_to_string(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }

    long size = ftell(fp);
    if (size <= 0) {
        fclose(fp);
        return NULL;
    }

    rewind(fp);

    char *buf = (char *)malloc((size_t)size + 1);
    if (buf == NULL) {
        fclose(fp);
        return NULL;
    }

    size_t read_size = fread(buf, 1, (size_t)size, fp);
    fclose(fp);

    if (read_size != (size_t)size) {
        free(buf);
        return NULL;
    }

    buf[size] = '\0';
    return buf;
}

/**
 * @brief  跳过字符串中的空白字符（空格、制表符、换行）
 * @param  p  原始字符串指针
 * @return 跳过空格后的新指针
 */
static const char *skip_spaces(const char *p)
{
    while (p != NULL && *p != '\0' && isspace((unsigned char)*p)) {
        p++;
    }

    return p;
}

/**
 * @brief  简易JSON解析：读取字符串类型值
 * @param  json      JSON字符串
 * @param  key       要读取的键名
 * @param  out       输出缓冲区
 * @param  out_size  输出缓冲区大小
 * @return 0成功，-1失败
 * @note   轻量解析，不依赖第三方JSON库
 */
static int json_get_string_simple(
    const char *json,
    const char *key,
    char *out,
    int out_size
)
{
    char pattern[128];

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *p = strstr(json, pattern);
    if (p == NULL) {
        return -1;
    }

    p += strlen(pattern);
    p = strchr(p, ':');
    if (p == NULL) {
        return -1;
    }

    p++;
    p = skip_spaces(p);

    if (*p != '"') {
        return -1;
    }

    p++;

    const char *end = strchr(p, '"');
    if (end == NULL) {
        return -1;
    }

    int len = (int)(end - p);
    if (len >= out_size) {
        len = out_size - 1;
    }

    strncpy(out, p, len);
    out[len] = '\0';

    return 0;
}

/**
 * @brief  简易JSON解析：读取整数类型值
 * @param  json   JSON字符串
 * @param  key    要读取的键名
 * @param  out    输出整数指针
 * @return 0成功，-1失败
 */
static int json_get_int_simple(const char *json, const char *key, int *out)
{
    char pattern[128];

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *p = strstr(json, pattern);
    if (p == NULL) {
        return -1;
    }

    p += strlen(pattern);
    p = strchr(p, ':');
    if (p == NULL) {
        return -1;
    }

    p++;
    p = skip_spaces(p);

    *out = atoi(p);
    return 0;
}

/**
 * @brief  加载设备配置（优先文件，无文件则用默认）
 * @param  config_path  配置文件路径（可为NULL）
 * @param  config       输出配置结构体
 * @return 0成功，-1失败
 */
int device_config_load(const char *config_path, DeviceConfig *config)
{
    if (config == NULL) {
        return -1;
    }

    // 先加载默认配置
    config_set_default(config);

    if (config_path == NULL) {
        LOG_WARN("config path is NULL, use default config");
        return 0;
    }

    // 读取配置文件
    char *json = read_file_to_string(config_path);
    if (json == NULL) {
        LOG_ERROR("read config failed: %s", config_path);
        return -1;
    }

    // 解析各项配置
    json_get_string_simple(json, "device_id",
                           config->device_id,
                           sizeof(config->device_id));

    json_get_string_simple(json, "server_ip",
                           config->server_ip,
                           sizeof(config->server_ip));

    json_get_int_simple(json, "server_port",
                        &config->server_port);

    json_get_string_simple(json, "test_image_path",
                           config->test_image_path,
                           sizeof(config->test_image_path));
    
    json_get_string_simple(json, "snapshot_dir",
                       config->snapshot_dir,
                       sizeof(config->snapshot_dir));

    json_get_string_simple(json, "log_level",
                           config->log_level,
                           sizeof(config->log_level));

    json_get_int_simple(json, "log_detail",
                        &config->log_detail);

    json_get_string_simple(json, "camera_dev",
                        config->camera_dev,
                        sizeof(config->camera_dev));

    json_get_int_simple(json, "image_width",
                        &config->image_width);

    json_get_int_simple(json, "image_height",
                        &config->image_height);

    free(json);

    // 端口合法性校验
    if (config->server_port <= 0 || config->server_port > 65535) {
        LOG_ERROR("invalid server_port: %d", config->server_port);
        return -1;
    }

    return 0;
}

/**
 * @brief  打印设备配置信息（方便调试）
 * @param  config  配置结构体指针
 * @return 无
 */
void device_config_print(const DeviceConfig *config)
{
    if (config == NULL) {
        return;
    }

    LOG_INFO("========== Device Config ==========");
    LOG_INFO("device_id       : %s", config->device_id);
    LOG_INFO("server_ip       : %s", config->server_ip);
    LOG_INFO("server_port     : %d", config->server_port);
    LOG_INFO("test_image_path : %s", config->test_image_path);
    LOG_INFO("snapshot_dir    : %s", config->snapshot_dir);
    LOG_INFO("log_level       : %s", config->log_level);
    LOG_INFO("log_detail      : %d", config->log_detail);
    LOG_INFO("camera_dev      : %s", config->camera_dev);
    LOG_INFO("image_width     : %d", config->image_width);
    LOG_INFO("image_height    : %d", config->image_height);
    LOG_INFO("===================================");
}
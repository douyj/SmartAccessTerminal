/**
 * @file        camera_mock.c
 * @brief       模拟摄像头模块（虚拟摄像头）
 * @details     不依赖硬件，通过复制本地图片模拟摄像头抓拍功能
 *              用于在无真实摄像头时调试门禁抓拍、上传、识别流程
 * @author      嵌入式驱动/设备模块
 * @date        2026
 */

#include "camera/camera.h"
#include "common/log.h"

#include <stdio.h>
#include <string.h>

/* 全局配置：模拟摄像头使用的源图片路径 */
static char g_source_image_path[CAMERA_SOURCE_PATH_MAX_LEN] = {0};
/* 全局状态：摄像头是否已初始化 */
static int g_camera_inited = 0;

/**
 * @brief  二进制文件拷贝工具函数（用于模拟抓拍）
 * @param  src_path  源图片路径（模板图片）
 * @param  dst_path  目标保存路径（抓拍输出）
 * @return 0 成功，-1 失败
 * @note   用于模拟摄像头抓拍：复制图片 = 拍照完成
 */
static int copy_file(const char *src_path, const char *dst_path)
{
    // 打开源文件（只读二进制）
    FILE *src = fopen(src_path, "rb");
    if (src == NULL) {
        LOG_ERROR("[CAMERA-MOCK] open source image failed: %s", src_path);
        return -1;
    }

    // 打开目标文件（只写二进制）
    FILE *dst = fopen(dst_path, "wb");
    if (dst == NULL) {
        LOG_ERROR("[CAMERA-MOCK] open dst image failed: %s", dst_path);
        fclose(src);
        return -1;
    }

    // 4KB 缓冲区循环拷贝
    char buf[4096];
    while (1) {
        // 读取一块数据
        size_t n = fread(buf, 1, sizeof(buf), src);

        // 有数据就写入目标文件
        if (n > 0) {
            size_t w = fwrite(buf, 1, n, dst);
            if (w != n) {
                LOG_ERROR("[CAMERA-MOCK] write dst image failed");
                fclose(src);
                fclose(dst);
                return -1;
            }
        }

        // 读取完毕或出错则退出
        if (n < sizeof(buf)) {
            if (ferror(src)) {
                LOG_ERROR("[CAMERA-MOCK] read source image failed");
                fclose(src);
                fclose(dst);
                return -1;
            }
            break;
        }
    }

    fclose(src);
    fclose(dst);
    return 0;
}


/**
 * @brief  初始化模拟摄像头
 * @param  source_image_path  模拟使用的模板图片路径（jpg）
 * @return 0 成功，-1 失败
 * @note   必须先调用此函数，才能抓拍
 */
int camera_init(const char *source_image_path)
{
    // 参数检查
    if (source_image_path == NULL || source_image_path[0] == '\0') {
        LOG_ERROR("[CAMERA-MOCK] invalid source image path");
        return -1;
    }

    // 保存源图片路径
    snprintf(g_source_image_path, sizeof(g_source_image_path), "%s", source_image_path);
    g_camera_inited = 1;

    LOG_INFO("[CAMERA-MOCK] init success, source image: %s", g_source_image_path);
    return 0;
}

/**
 * @brief  模拟抓拍一张 JPEG 图片
 * @param  save_path  抓拍图片的保存路径
 * @return 0 成功，-1 失败
 * @note   本质：复制模板图片 -> 保存路径 = 抓拍完成
 */
int camera_capture_jpeg(const char *save_path)
{
    // 状态检查
    if (!g_camera_inited) {
        LOG_ERROR("[CAMERA-MOCK] camera not initialized");
        return -1;
    }

    // 参数检查
    if (save_path == NULL || save_path[0] == '\0') {
        LOG_ERROR("[CAMERA-MOCK] invalid save path");
        return -1;
    }

    LOG_INFO("[CAMERA-MOCK] capture jpeg");
    LOG_INFO("[CAMERA-MOCK] copy %s -> %s", g_source_image_path, save_path);

    // 复制图片 = 模拟抓拍
    if (copy_file(g_source_image_path, save_path) < 0) {
        LOG_ERROR("[CAMERA-MOCK] capture failed");
        return -1;
    }

    LOG_INFO("[CAMERA-MOCK] capture success: %s", save_path);
    return 0;
}

/**
 * @brief  反初始化模拟摄像头，释放资源
 * @return 0 成功
 */
int camera_deinit(void)
{
    if (!g_camera_inited) {
        return 0;
    }

    LOG_INFO("[CAMERA-MOCK] deinit");

    // 清空状态
    g_camera_inited = 0;
    g_source_image_path[0] = '\0';

    return 0;
}
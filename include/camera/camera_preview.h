#ifndef CAMERA_PREVIEW_H
#define CAMERA_PREVIEW_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动实时预览采集线程
 * @param dev_name 摄像头设备，例如 /dev/video1
 * @param width    期望宽度
 * @param height   期望高度
 * @return 0成功，-1失败
 */
int camera_preview_start(const char *dev_name, int width, int height);

/**
 * @brief 停止实时预览采集线程并释放资源
 */
void camera_preview_stop(void);

/**
 * @brief 获取最新一帧 RGB565 数据
 * @param out_buf   输出缓冲区
 * @param out_size  输出缓冲区大小
 * @param width     返回帧宽度
 * @param height    返回帧高度
 * @param frame_size 返回帧数据大小
 * @return 0成功，-1失败
 */
int camera_preview_get_frame(unsigned char *out_buf,
                             int out_size,
                             int *width,
                             int *height,
                             int *frame_size);

/**
 * @brief 获取当前预览信息
 */
int camera_preview_get_info(int *width, int *height, int *frame_size);

/**
 * @brief 是否正在运行
 */
int camera_preview_is_running(void);

/**
 * @brief 获取最近统计的 FPS
 */
int camera_preview_get_fps(void);

#ifdef __cplusplus
}
#endif

#endif

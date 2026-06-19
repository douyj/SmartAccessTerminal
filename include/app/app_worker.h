#ifndef APP_WORKER_H
#define APP_WORKER_H

#include "config/device_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 旧版 worker：
 * 自己调用 camera_capture_jpeg() 抓拍。
 */
int app_worker_start(const DeviceConfig *config);

/*
 * 第22关新版 worker：
 * 不再直接操作摄像头，而是从 camera_preview_get_frame() 抽取当前帧上传。
 */
int app_worker_start_from_preview(const DeviceConfig *config);

void app_worker_stop(void);
int app_worker_is_running(void);

#ifdef __cplusplus
}
#endif

#endif

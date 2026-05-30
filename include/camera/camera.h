#ifndef CAMERA_H
#define CAMERA_H

int camera_init(const char *dev_name, int width, int height);
int camera_capture_jpeg(const char *save_path);
int camera_deinit(void);

#endif

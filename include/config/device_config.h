#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#define DEVICE_ID_MAX_LEN      64
#define SERVER_IP_MAX_LEN      64
#define IMAGE_PATH_MAX_LEN     256
#define SNAPSHOT_DIR_MAX_LEN   256
#define CAMERA_DEV_MAX_LEN     64
#define LOG_LEVEL_MAX_LEN      16

typedef struct {
    char device_id[DEVICE_ID_MAX_LEN];

    char server_ip[SERVER_IP_MAX_LEN];
    int server_port;

    char camera_dev[CAMERA_DEV_MAX_LEN];
    int image_width;
    int image_height;

    char test_image_path[IMAGE_PATH_MAX_LEN];
    char snapshot_dir[SNAPSHOT_DIR_MAX_LEN];

    char log_level[LOG_LEVEL_MAX_LEN];
    int log_detail;
} DeviceConfig;

int device_config_load(const char *config_path, DeviceConfig *config);
void device_config_print(const DeviceConfig *config);

#endif
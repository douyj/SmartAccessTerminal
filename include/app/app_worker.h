#ifndef APP_WORKER_H
#define APP_WORKER_H

#include "config/device_config.h"

#ifdef __cplusplus
extern "C" {
#endif

int app_worker_start(const DeviceConfig *config);
void app_worker_stop(void);
int app_worker_is_running(void);

#ifdef __cplusplus
}
#endif

#endif

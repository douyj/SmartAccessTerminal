#ifndef APP_TRIGGER_H
#define APP_TRIGGER_H

#ifdef __cplusplus
extern "C" {
#endif

void app_trigger_init(void);
void app_trigger_request(void);
int app_trigger_wait(int timeout_ms);
void app_trigger_stop(void);

#ifdef __cplusplus
}
#endif

#endif

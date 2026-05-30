#ifndef UI_MAIN_H
#define UI_MAIN_H

void ui_main_create(void);

void ui_set_device_info(const char *device_id,
                        const char *server,
                        const char *camera);

void ui_set_status(const char *status);
void ui_show_result(const char *result,
                    const char *name,
                    float confidence,
                    const char *action);

void ui_show_error(const char *error_msg);

#endif

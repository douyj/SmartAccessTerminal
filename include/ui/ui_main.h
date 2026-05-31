#ifndef UI_MAIN_H
#define UI_MAIN_H

#include "app/app_state.h"

typedef enum {
    UI_STATE_IDLE = 0,
    UI_STATE_CAPTURING,
    UI_STATE_UPLOADING,
    UI_STATE_VERIFYING,
    UI_STATE_SUCCESS,
    UI_STATE_DENY,
    UI_STATE_NO_FACE,
    UI_STATE_ERROR
} UiState;

void ui_main_create(void);

void ui_set_top_info(const char *time_str, int wifi_ok, int tcp_online);

void ui_set_state(UiState state);

void ui_show_success(const char *name, float confidence, int auto_close_sec);
void ui_show_deny(const char *name, float confidence);
void ui_show_no_face(void);
void ui_show_error(const char *msg);

void ui_hide_result_card(void);

/*
 * UI 线程从 app_state 读取状态并刷新界面。
 */
void ui_update_from_app_state(void);

#endif
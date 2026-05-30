#ifndef UI_MAIN_H
#define UI_MAIN_H

typedef enum {
    UI_STATE_IDLE = 0,        // 空闲待机（默认界面）
    UI_STATE_CAPTURING,       // 正在抓拍人脸
    UI_STATE_UPLOADING,       // 正在上传人脸数据
    UI_STATE_VERIFYING,       // 服务端校验中
    UI_STATE_SUCCESS,         // 识别成功，开门
    UI_STATE_DENY,            // 识别拒绝（人脸不匹配）
    UI_STATE_NO_FACE,         // 未检测到人脸
    UI_STATE_ERROR            // 系统/网络/功能异常
} UiState;

//初始化
void ui_main_create(void);

//顶部栏
void ui_set_top_info(const char *time_str, int wifi_ok, int tcp_online);

//设置ui状态
void ui_set_state(UiState state);

//显示状态栏
void ui_show_success(const char *name, float confidence, int auto_close_sec);
void ui_show_deny(const char *name, float confidence);
void ui_show_no_face(void);
void ui_show_error(const char *msg);

#endif
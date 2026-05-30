#include "ui/ui_main.h"

#include "lvgl/lvgl.h"

#include <stdio.h>
#include <string.h>

/* ===================== 颜色宏 ===================== */
#define COLOR_BG        0x06111F
#define COLOR_PANEL     0x102033
#define COLOR_PANEL_2   0x16263A
#define COLOR_BLUE      0x1E90FF
#define COLOR_CYAN      0x00D1FF
#define COLOR_GREEN     0x20E070
#define COLOR_RED       0xFF4D4D
#define COLOR_YELLOW    0xFFD166
#define COLOR_WHITE     0xFFFFFF
#define COLOR_GRAY      0xA8B3C2

/* ===================== 全局控件指针 ===================== */
static lv_obj_t *g_label_time = NULL;
static lv_obj_t *g_label_wifi = NULL;
static lv_obj_t *g_label_tcp = NULL;

static lv_obj_t *g_camera_area = NULL;
static lv_obj_t *g_face_box = NULL;
static lv_obj_t *g_label_scan = NULL;

static lv_obj_t *g_result_card = NULL;      /*结果面板容器*/
static lv_obj_t *g_label_icon = NULL;       /*状态图标*/
static lv_obj_t *g_label_title = NULL;      /*状态标题文本*/
static lv_obj_t *g_label_name = NULL;       /*用户姓名*/
static lv_obj_t *g_label_desc = NULL;       /*置信度*/
static lv_obj_t *g_label_action = NULL;     /*门锁动作提示文本*/


/**
 * @brief       设置标签文本颜色
 * @param[in]   obj     LVGL 标签控件句柄
 * @param[in]   color   十六进制 RGB 颜色值
 * @retval      无
 */
static void set_text_color(lv_obj_t *obj, unsigned int color)
{
    if (obj) {
        lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    }
}

/**
 * @brief       快速创建文本标签封装函数
 * @param[in]   parent  父容器句柄
 * @param[in]   text    标签显示文本
 * @param[in]   x       X 坐标
 * @param[in]   y       Y 坐标
 * @param[in]   color   文本颜色
 * @param[in]   font    字体句柄，NULL 使用默认字体
 * @retval      lv_obj_t* 新建标签控件句柄
 */
static lv_obj_t *make_label(lv_obj_t *parent,
                            const char *text,
                            int x,
                            int y,
                            unsigned int color,
                            const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);

    if (font) {
        lv_obj_set_style_text_font(label, font, 0);
    }

    return label;
}


/**
 * @brief       人脸框动画回调函数
 * @param[in]   obj     动画目标控件（人脸框）
 * @param[in]   value   动画当前透明度值
 * @retval      无
 */
static void face_box_anim_cb(void *obj, int32_t value)
{
    lv_obj_set_style_border_opa((lv_obj_t *)obj, value, 0);
}


/**
 * @brief       启用人脸框呼吸动画（扫描效果）
 * @retval      无
 * @note        循环往返动画，模拟 AI 扫描状态
 */
static void start_face_box_anim(void)
{
    if (!g_face_box) {
        return;
    }

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, g_face_box);
    lv_anim_set_values(&anim, LV_OPA_40, LV_OPA_COVER);
    lv_anim_set_time(&anim, 700);
    lv_anim_set_playback_time(&anim, 700);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim, face_box_anim_cb);
    lv_anim_start(&anim);
}

/**
 * @brief       创建顶部状态栏（时间 + WiFi + TCP 状态）
 * @param[in]   scr     主屏幕句柄
 * @retval      无
 */
static void create_top_bar(lv_obj_t *scr)
{
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_size(bar, 480, 58);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x07111F), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_80, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    g_label_time = make_label(bar, "23:13", 22, 17, COLOR_WHITE, &lv_font_montserrat_20);

    g_label_wifi = make_label(bar, "WiFi", 315, 20, COLOR_GREEN, &lv_font_montserrat_14);
    g_label_tcp = make_label(bar, "TCP", 385, 20, COLOR_GREEN, &lv_font_montserrat_14);
}

/**
 * @brief       创建相机预览区域（预览框 + 人脸框 + 扫描提示）
 * @param[in]   scr     主屏幕句柄
 * @retval      无
 */
static void create_camera_area(lv_obj_t *scr)
{
    g_camera_area = lv_obj_create(scr);
    lv_obj_set_size(g_camera_area, 480, 800);
    lv_obj_set_pos(g_camera_area, 0, 0);
    lv_obj_set_style_bg_color(g_camera_area, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_border_width(g_camera_area, 0, 0);
    lv_obj_clear_flag(g_camera_area, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *preview = lv_obj_create(g_camera_area);
    lv_obj_set_size(preview, 440, 520);
    lv_obj_set_pos(preview, 20, 78);
    lv_obj_set_style_radius(preview, 20, 0);
    lv_obj_set_style_bg_color(preview, lv_color_hex(0x0B1828), 0);
    lv_obj_set_style_border_color(preview, lv_color_hex(0x1A3555), 0);
    lv_obj_set_style_border_width(preview, 1, 0);
    lv_obj_clear_flag(preview, LV_OBJ_FLAG_SCROLLABLE);

    make_label(preview, "CAMERA PREVIEW", 130, 35, 0x314C70, &lv_font_montserrat_16);

    g_face_box = lv_obj_create(preview);
    lv_obj_set_size(g_face_box, 220, 260);
    lv_obj_set_pos(g_face_box, 110, 130);
    lv_obj_set_style_bg_opa(g_face_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(g_face_box, lv_color_hex(COLOR_BLUE), 0);
    lv_obj_set_style_border_width(g_face_box, 3, 0);
    lv_obj_set_style_radius(g_face_box, 14, 0);
    lv_obj_clear_flag(g_face_box, LV_OBJ_FLAG_SCROLLABLE);

    g_label_scan = make_label(preview, "AI SCANNING", 158, 405, COLOR_CYAN, &lv_font_montserrat_16);

    start_face_box_anim();
}

/**
 * @brief       创建底部识别结果面板
 * @param[in]   scr     主屏幕句柄
 * @retval      无
 */
static void create_result_card(lv_obj_t *scr)
{
    g_result_card = lv_obj_create(scr);
    lv_obj_set_size(g_result_card, 420, 150);
    lv_obj_set_pos(g_result_card, 30, 620);
    lv_obj_set_style_radius(g_result_card, 22, 0);
    lv_obj_set_style_bg_color(g_result_card, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(g_result_card, LV_OPA_90, 0);
    lv_obj_set_style_border_color(g_result_card, lv_color_hex(0x244B76), 0);
    lv_obj_set_style_border_width(g_result_card, 1, 0);
    lv_obj_clear_flag(g_result_card, LV_OBJ_FLAG_SCROLLABLE);

    g_label_icon = make_label(g_result_card, "●", 24, 25, COLOR_CYAN, &lv_font_montserrat_28);
    g_label_title = make_label(g_result_card, "Waiting", 70, 22, COLOR_WHITE, &lv_font_montserrat_24);
    g_label_name = make_label(g_result_card, "Please face the camera", 72, 62, COLOR_GRAY, &lv_font_montserrat_16);
    g_label_desc = make_label(g_result_card, "System ready", 72, 92, COLOR_GRAY, &lv_font_montserrat_14);
    g_label_action = make_label(g_result_card, "Door: Closed", 260, 95, COLOR_CYAN, &lv_font_montserrat_14);
}


/**
 * @brief       初始化并创建整个主界面
 * @retval      无
 * @note        页面创建顺序：相机区 -> 状态栏 -> 结果面板，最后切为空闲状态
 */
void ui_main_create(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), 0);

    create_camera_area(scr);
    create_top_bar(scr);
    create_result_card(scr);

    ui_set_state(UI_STATE_IDLE);
}

/**
 * @brief       更新顶部状态栏信息：时间、WiFi、TCP 在线状态
 * @param[in]   time_str    时间字符串
 * @param[in]   wifi_ok     WiFi 状态：1-正常 0-异常
 * @param[in]   tcp_online  TCP 连接状态：1-在线 0-离线
 * @retval      无
 */
void ui_set_top_info(const char *time_str, int wifi_ok, int tcp_online)
{
    if (g_label_time && time_str) {
        lv_label_set_text(g_label_time, time_str);
    }

    if (g_label_wifi) {
        lv_label_set_text(g_label_wifi, wifi_ok ? "WiFi" : "WiFi X");
        set_text_color(g_label_wifi, wifi_ok ? COLOR_GREEN : COLOR_RED);
    }

    if (g_label_tcp) {
        lv_label_set_text(g_label_tcp, tcp_online ? "TCP ON" : "TCP OFF");
        set_text_color(g_label_tcp, tcp_online ? COLOR_GREEN : COLOR_RED);
    }
}

/**
 * @brief       统一设置界面整体工作状态
 * @param[in]   state   界面状态枚举 @ref UiState
 * @retval      无
 */
void ui_set_state(UiState state)
{
    if (!g_label_title) {
        return;
    }

    switch (state) {
    case UI_STATE_IDLE:
        lv_label_set_text(g_label_icon, "●");
        lv_label_set_text(g_label_title, "Waiting");
        lv_label_set_text(g_label_name, "Please face the camera");
        lv_label_set_text(g_label_desc, "System ready");
        lv_label_set_text(g_label_action, "Door: Closed");
        set_text_color(g_label_icon, COLOR_CYAN);
        set_text_color(g_label_title, COLOR_WHITE);
        break;

    case UI_STATE_CAPTURING:
        lv_label_set_text(g_label_icon, "●");
        lv_label_set_text(g_label_title, "Capturing");
        lv_label_set_text(g_label_name, "Camera frame captured");
        lv_label_set_text(g_label_desc, "Preparing image");
        lv_label_set_text(g_label_action, "Door: Closed");
        set_text_color(g_label_icon, COLOR_BLUE);
        set_text_color(g_label_title, COLOR_CYAN);
        break;

    case UI_STATE_UPLOADING:
        lv_label_set_text(g_label_icon, "●");
        lv_label_set_text(g_label_title, "Uploading");
        lv_label_set_text(g_label_name, "Sending frame to server");
        lv_label_set_text(g_label_desc, "TCP transmission");
        lv_label_set_text(g_label_action, "Door: Closed");
        set_text_color(g_label_icon, COLOR_BLUE);
        set_text_color(g_label_title, COLOR_CYAN);
        break;

    case UI_STATE_VERIFYING:
        lv_label_set_text(g_label_icon, "●");
        lv_label_set_text(g_label_title, "Verifying");
        lv_label_set_text(g_label_name, "AI recognition running");
        lv_label_set_text(g_label_desc, "Waiting for result");
        lv_label_set_text(g_label_action, "Door: Closed");
        set_text_color(g_label_icon, COLOR_YELLOW);
        set_text_color(g_label_title, COLOR_YELLOW);
        break;

    case UI_STATE_SUCCESS:
        ui_show_success("Deng Yangjie", 0.96f, 3);
        break;

    case UI_STATE_DENY:
        ui_show_deny("Unknown", 0.30f);
        break;

    case UI_STATE_NO_FACE:
        ui_show_no_face();
        break;

    case UI_STATE_ERROR:
        ui_show_error("Network error");
        break;

    default:
        break;
    }
}

/**
 * @brief       显示识别成功界面
 * @param[in]   name            识别到的人员姓名
 * @param[in]   confidence      人脸识别置信度 0~1.0
 * @param[in]   auto_close_sec  门自动关闭倒计时(秒)
 * @retval      无
 */
void ui_show_success(const char *name, float confidence, int auto_close_sec)
{
    char buf[128];

    lv_label_set_text(g_label_icon, "✓");
    lv_label_set_text(g_label_title, "Identity Verified");

    snprintf(buf, sizeof(buf), "%s", name ? name : "Unknown");
    lv_label_set_text(g_label_name, buf);

    snprintf(buf, sizeof(buf), "Confidence %.2f", confidence);
    lv_label_set_text(g_label_desc, buf);

    snprintf(buf, sizeof(buf), "Door Open  %ds", auto_close_sec);
    lv_label_set_text(g_label_action, buf);

    set_text_color(g_label_icon, COLOR_GREEN);
    set_text_color(g_label_title, COLOR_GREEN);
    set_text_color(g_label_action, COLOR_GREEN);
}

/**
 * @brief       显示识别拒绝界面
 * @param[in]   name        人员名称
 * @param[in]   confidence  人脸识别置信度 0~1.0
 * @retval      无
 */
void ui_show_deny(const char *name, float confidence)
{
    char buf[128];

    lv_label_set_text(g_label_icon, "×");
    lv_label_set_text(g_label_title, "Access Denied");

    snprintf(buf, sizeof(buf), "%s", name ? name : "Unknown");
    lv_label_set_text(g_label_name, buf);

    snprintf(buf, sizeof(buf), "Confidence %.2f", confidence);
    lv_label_set_text(g_label_desc, buf);

    lv_label_set_text(g_label_action, "Alarm Triggered");

    set_text_color(g_label_icon, COLOR_RED);
    set_text_color(g_label_title, COLOR_RED);
    set_text_color(g_label_action, COLOR_RED);
}

/**
 * @brief       显示未检测到人脸提示
 * @retval      无
 */
void ui_show_no_face(void)
{
    lv_label_set_text(g_label_icon, "!");
    lv_label_set_text(g_label_title, "No Face");
    lv_label_set_text(g_label_name, "Please face the camera");
    lv_label_set_text(g_label_desc, "Waiting for valid face");
    lv_label_set_text(g_label_action, "Door: Closed");

    set_text_color(g_label_icon, COLOR_YELLOW);
    set_text_color(g_label_title, COLOR_YELLOW);
    set_text_color(g_label_action, COLOR_YELLOW);
}

/**
 * @brief       显示系统/网络错误提示
 * @param[in]   msg 错误描述信息
 * @retval      无
 */
void ui_show_error(const char *msg)
{
    lv_label_set_text(g_label_icon, "!");
    lv_label_set_text(g_label_title, "System Error");
    lv_label_set_text(g_label_name, msg ? msg : "Unknown error");
    lv_label_set_text(g_label_desc, "Check network or device");
    lv_label_set_text(g_label_action, "Door: Closed");

    set_text_color(g_label_icon, COLOR_RED);
    set_text_color(g_label_title, COLOR_RED);
    set_text_color(g_label_action, COLOR_RED);
}

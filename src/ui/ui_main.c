#include "ui/ui_main.h"

#include "lvgl/lvgl.h"

#include <stdio.h>
#include <string.h>

#define COLOR_BG        0x06111F
#define COLOR_PANEL     0x102033
#define COLOR_BLUE      0x1E90FF
#define COLOR_CYAN      0x00D1FF
#define COLOR_GREEN     0x20E070
#define COLOR_RED       0xFF4D4D
#define COLOR_YELLOW    0xFFD166
#define COLOR_WHITE     0xFFFFFF
#define COLOR_GRAY      0xA8B3C2

static lv_obj_t *g_video_layer = NULL;
static lv_obj_t *g_overlay_layer = NULL;

static lv_obj_t *g_label_time = NULL;
static lv_obj_t *g_label_wifi = NULL;
static lv_obj_t *g_label_tcp = NULL;

static lv_obj_t *g_label_status = NULL;

static lv_obj_t *g_result_card = NULL;

static lv_obj_t *g_face_thumb = NULL;
static lv_obj_t *g_face_thumb_label = NULL;

static lv_obj_t *g_text_group = NULL;
static lv_obj_t *g_label_title = NULL;
static lv_obj_t *g_label_name = NULL;
static lv_obj_t *g_label_desc = NULL;

static lv_obj_t *g_action_pill = NULL;
static lv_obj_t *g_label_action = NULL;

static lv_timer_t *g_hide_card_timer = NULL;


static void set_text_color(lv_obj_t *obj, unsigned int color)
{
    if (obj) {
        lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    }
}


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


void ui_hide_result_card(void);


static void hide_card_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    ui_hide_result_card();

    if (g_hide_card_timer) {
        lv_timer_del(g_hide_card_timer);
        g_hide_card_timer = NULL;
    }
}


static void show_result_card_for_ms(unsigned int ms)
{
    if (!g_result_card) {
        return;
    }

    lv_obj_clear_flag(g_result_card, LV_OBJ_FLAG_HIDDEN);

    if (g_hide_card_timer) {
        lv_timer_del(g_hide_card_timer);
        g_hide_card_timer = NULL;
    }

    g_hide_card_timer = lv_timer_create(hide_card_timer_cb, ms, NULL);
    lv_timer_set_repeat_count(g_hide_card_timer, 1);
}


static void create_video_layer(lv_obj_t *scr)
{
    /*
     * 当前先用深色背景模拟全屏实时视频。
     * 后面接 RGB565 实时画面时，这里可以换成 lv_img。
     */
    g_video_layer = lv_obj_create(scr);
    lv_obj_set_size(g_video_layer, 480, 800);
    lv_obj_set_pos(g_video_layer, 0, 0);

    lv_obj_set_style_bg_color(g_video_layer, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_border_width(g_video_layer, 0, 0);
    lv_obj_set_style_pad_all(g_video_layer, 0, 0);
    lv_obj_clear_flag(g_video_layer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *mock_text = lv_label_create(g_video_layer);
    lv_label_set_text(mock_text, "LIVE CAMERA");
    lv_obj_set_style_text_color(mock_text, lv_color_hex(0x1A3555), 0);
    lv_obj_set_style_text_font(mock_text, &lv_font_montserrat_24, 0);
    lv_obj_align(mock_text, LV_ALIGN_CENTER, 0, -120);
}


static void create_overlay_layer(lv_obj_t *scr)
{
    /*
     * 全屏透明悬浮层。
     * 顶部栏、状态文字、底部识别卡片都放在这一层。
     */
    g_overlay_layer = lv_obj_create(scr);
    lv_obj_set_size(g_overlay_layer, 480, 800);
    lv_obj_set_pos(g_overlay_layer, 0, 0);

    lv_obj_set_style_bg_opa(g_overlay_layer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_overlay_layer, 0, 0);
    lv_obj_set_style_pad_all(g_overlay_layer, 0, 0);
    lv_obj_clear_flag(g_overlay_layer, LV_OBJ_FLAG_SCROLLABLE);
}


static void create_top_bar(lv_obj_t *parent)
{
    /*
     * 顶部状态栏悬浮在视频上。
     */
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, 440, 48);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 16);

    lv_obj_set_style_radius(bar, 18, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x07111F), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_60, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    g_label_time = make_label(bar,
                              "23:13",
                              18,
                              13,
                              COLOR_WHITE,
                              &lv_font_montserrat_18);

    g_label_wifi = make_label(bar,
                              "WiFi",
                              280,
                              15,
                              COLOR_GREEN,
                              &lv_font_montserrat_14);

    g_label_tcp = make_label(bar,
                             "TCP ON",
                             340,
                             15,
                             COLOR_GREEN,
                             &lv_font_montserrat_14);
}


static void create_center_status(lv_obj_t *parent)
{
    /*
     * 中间悬浮状态文字。
     * 过程状态只更新这里，不弹出底部结果卡片。
     */
    g_label_status = lv_label_create(parent);
    lv_label_set_text(g_label_status, "AI SCANNING");
    lv_obj_set_style_text_color(g_label_status, lv_color_hex(COLOR_CYAN), 0);
    lv_obj_set_style_text_font(g_label_status, &lv_font_montserrat_20, 0);
    lv_obj_align(g_label_status, LV_ALIGN_CENTER, 0, 170);
}


static void create_result_card(lv_obj_t *parent)
{
    /*
     * 底部悬浮识别卡片。
     * 屏幕宽 480，卡片宽 408，左右边距约 36。
     * 使用 LV_ALIGN_BOTTOM_MID 自动居中，避免肉眼看到左右不一致。
     */
    g_result_card = lv_obj_create(parent);
    lv_obj_set_size(g_result_card, 408, 148);
    lv_obj_align(g_result_card, LV_ALIGN_BOTTOM_MID, 0, -26);

    lv_obj_set_style_radius(g_result_card, 24, 0);
    lv_obj_set_style_bg_color(g_result_card, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(g_result_card, LV_OPA_80, 0);
    lv_obj_set_style_border_color(g_result_card, lv_color_hex(COLOR_BLUE), 0);
    lv_obj_set_style_border_opa(g_result_card, LV_OPA_50, 0);
    lv_obj_set_style_border_width(g_result_card, 1, 0);
    lv_obj_set_style_pad_all(g_result_card, 0, 0);
    lv_obj_clear_flag(g_result_card, LV_OBJ_FLAG_SCROLLABLE);

    /*
     * 左侧识别帧缩略图区域。
     * 后续这里会替换成后台识别那一帧画面。
     */
    g_face_thumb = lv_obj_create(g_result_card);
    lv_obj_set_size(g_face_thumb, 88, 96);
    lv_obj_set_pos(g_face_thumb, 24, 26);

    lv_obj_set_style_radius(g_face_thumb, 16, 0);
    lv_obj_set_style_bg_color(g_face_thumb, lv_color_hex(0x0B1828), 0);
    lv_obj_set_style_bg_opa(g_face_thumb, LV_OPA_90, 0);
    lv_obj_set_style_border_color(g_face_thumb, lv_color_hex(COLOR_BLUE), 0);
    lv_obj_set_style_border_opa(g_face_thumb, LV_OPA_80, 0);
    lv_obj_set_style_border_width(g_face_thumb, 1, 0);
    lv_obj_set_style_pad_all(g_face_thumb, 0, 0);
    lv_obj_clear_flag(g_face_thumb, LV_OBJ_FLAG_SCROLLABLE);

    g_face_thumb_label = lv_label_create(g_face_thumb);
    lv_label_set_text(g_face_thumb_label, "FACE");
    lv_obj_set_style_text_color(g_face_thumb_label, lv_color_hex(0x315C88), 0);
    lv_obj_set_style_text_font(g_face_thumb_label, &lv_font_montserrat_14, 0);
    lv_obj_center(g_face_thumb_label);

    /*
     * 右侧文字整体区域。
     * 做成一个透明 group，便于整体排版。
     */
    g_text_group = lv_obj_create(g_result_card);
    lv_obj_set_size(g_text_group, 250, 104);
    lv_obj_set_pos(g_text_group, 132, 24);

    lv_obj_set_style_bg_opa(g_text_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_text_group, 0, 0);
    lv_obj_set_style_pad_all(g_text_group, 0, 0);
    lv_obj_clear_flag(g_text_group, LV_OBJ_FLAG_SCROLLABLE);

    g_label_title = make_label(g_text_group,
                               "Identity Verified",
                               0,
                               0,
                               COLOR_GREEN,
                               &lv_font_montserrat_20);

    g_label_name = make_label(g_text_group,
                              "Deng Yangjie",
                              0,
                              38,
                              COLOR_WHITE,
                              &lv_font_montserrat_18);

    g_label_desc = make_label(g_text_group,
                              "Confidence 0.96",
                              0,
                              70,
                              COLOR_GRAY,
                              &lv_font_montserrat_14);

    /*
     * 右下角动作状态胶囊。
     */
    g_action_pill = lv_obj_create(g_text_group);
    lv_obj_set_size(g_action_pill, 116, 30);
    lv_obj_set_pos(g_action_pill, 130, 66);

    lv_obj_set_style_radius(g_action_pill, 15, 0);
    lv_obj_set_style_bg_color(g_action_pill, lv_color_hex(0x0E2A1C), 0);
    lv_obj_set_style_bg_opa(g_action_pill, LV_OPA_80, 0);
    lv_obj_set_style_border_width(g_action_pill, 0, 0);
    lv_obj_set_style_pad_all(g_action_pill, 0, 0);
    lv_obj_clear_flag(g_action_pill, LV_OBJ_FLAG_SCROLLABLE);

    g_label_action = lv_label_create(g_action_pill);
    lv_label_set_text(g_label_action, "Door Open 3s");
    lv_obj_set_style_text_color(g_label_action, lv_color_hex(COLOR_GREEN), 0);
    lv_obj_set_style_text_font(g_label_action, &lv_font_montserrat_14, 0);
    lv_obj_center(g_label_action);

    /*
     * 默认隐藏。
     * 只有收到识别结果后才显示几秒。
     */
    lv_obj_add_flag(g_result_card, LV_OBJ_FLAG_HIDDEN);
}


void ui_main_create(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), 0);

    create_video_layer(scr);
    create_overlay_layer(scr);

    create_top_bar(g_overlay_layer);
    create_center_status(g_overlay_layer);
    create_result_card(g_overlay_layer);

    ui_set_state(UI_STATE_IDLE);
}


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


void ui_set_state(UiState state)
{
    if (!g_label_status) {
        return;
    }

    /*
     * 过程状态只更新中间状态文字。
     * 不显示底部结果卡片。
     */
    switch (state) {
    case UI_STATE_IDLE:
        lv_label_set_text(g_label_status, "AI SCANNING");
        set_text_color(g_label_status, COLOR_CYAN);
        break;

    case UI_STATE_CAPTURING:
        lv_label_set_text(g_label_status, "CAPTURING");
        set_text_color(g_label_status, COLOR_CYAN);
        break;

    case UI_STATE_UPLOADING:
        lv_label_set_text(g_label_status, "UPLOADING");
        set_text_color(g_label_status, COLOR_BLUE);
        break;

    case UI_STATE_VERIFYING:
        lv_label_set_text(g_label_status, "VERIFYING");
        set_text_color(g_label_status, COLOR_YELLOW);
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


void ui_show_success(const char *name, float confidence, int auto_close_sec)
{
    char buf[128];

    if (!g_result_card) {
        return;
    }

    if (g_face_thumb_label) {
        lv_label_set_text(g_face_thumb_label, "FACE");
        set_text_color(g_face_thumb_label, COLOR_GREEN);
    }

    if (g_face_thumb) {
        lv_obj_set_style_border_color(g_face_thumb, lv_color_hex(COLOR_GREEN), 0);
    }

    lv_label_set_text(g_label_title, "Identity Verified");

    snprintf(buf, sizeof(buf), "%s", name ? name : "Unknown");
    lv_label_set_text(g_label_name, buf);

    snprintf(buf, sizeof(buf), "Confidence %.2f", confidence);
    lv_label_set_text(g_label_desc, buf);

    snprintf(buf, sizeof(buf), "Door Open %ds", auto_close_sec);
    lv_label_set_text(g_label_action, buf);

    set_text_color(g_label_title, COLOR_GREEN);
    set_text_color(g_label_name, COLOR_WHITE);
    set_text_color(g_label_desc, COLOR_GRAY);
    set_text_color(g_label_action, COLOR_GREEN);

    if (g_action_pill) {
        lv_obj_set_style_bg_color(g_action_pill, lv_color_hex(0x0E2A1C), 0);
    }

    set_text_color(g_label_status, COLOR_GREEN);
    lv_label_set_text(g_label_status, "ACCESS GRANTED");

    show_result_card_for_ms(3000);
}


void ui_show_deny(const char *name, float confidence)
{
    char buf[128];

    if (!g_result_card) {
        return;
    }

    if (g_face_thumb_label) {
        lv_label_set_text(g_face_thumb_label, "FACE");
        set_text_color(g_face_thumb_label, COLOR_RED);
    }

    if (g_face_thumb) {
        lv_obj_set_style_border_color(g_face_thumb, lv_color_hex(COLOR_RED), 0);
    }

    lv_label_set_text(g_label_title, "Access Denied");

    snprintf(buf, sizeof(buf), "%s", name ? name : "Unknown");
    lv_label_set_text(g_label_name, buf);

    snprintf(buf, sizeof(buf), "Confidence %.2f", confidence);
    lv_label_set_text(g_label_desc, buf);

    lv_label_set_text(g_label_action, "Alarm");

    set_text_color(g_label_title, COLOR_RED);
    set_text_color(g_label_name, COLOR_WHITE);
    set_text_color(g_label_desc, COLOR_GRAY);
    set_text_color(g_label_action, COLOR_RED);

    if (g_action_pill) {
        lv_obj_set_style_bg_color(g_action_pill, lv_color_hex(0x35151A), 0);
    }

    set_text_color(g_label_status, COLOR_RED);
    lv_label_set_text(g_label_status, "ACCESS DENIED");

    show_result_card_for_ms(3000);
}


void ui_show_no_face(void)
{
    if (!g_result_card) {
        return;
    }

    if (g_face_thumb_label) {
        lv_label_set_text(g_face_thumb_label, "NO\nFACE");
        set_text_color(g_face_thumb_label, COLOR_YELLOW);
    }

    if (g_face_thumb) {
        lv_obj_set_style_border_color(g_face_thumb, lv_color_hex(COLOR_YELLOW), 0);
    }

    lv_label_set_text(g_label_title, "No Face");
    lv_label_set_text(g_label_name, "Please face camera");
    lv_label_set_text(g_label_desc, "Waiting for valid face");
    lv_label_set_text(g_label_action, "Closed");

    set_text_color(g_label_title, COLOR_YELLOW);
    set_text_color(g_label_name, COLOR_WHITE);
    set_text_color(g_label_desc, COLOR_GRAY);
    set_text_color(g_label_action, COLOR_YELLOW);

    if (g_action_pill) {
        lv_obj_set_style_bg_color(g_action_pill, lv_color_hex(0x332A12), 0);
    }

    set_text_color(g_label_status, COLOR_YELLOW);
    lv_label_set_text(g_label_status, "NO FACE");

    show_result_card_for_ms(2500);
}


void ui_show_error(const char *msg)
{
    if (!g_result_card) {
        return;
    }

    if (g_face_thumb_label) {
        lv_label_set_text(g_face_thumb_label, "ERR");
        set_text_color(g_face_thumb_label, COLOR_RED);
    }

    if (g_face_thumb) {
        lv_obj_set_style_border_color(g_face_thumb, lv_color_hex(COLOR_RED), 0);
    }

    lv_label_set_text(g_label_title, "System Error");
    lv_label_set_text(g_label_name, msg ? msg : "Unknown error");
    lv_label_set_text(g_label_desc, "Check network");
    lv_label_set_text(g_label_action, "Closed");

    set_text_color(g_label_title, COLOR_RED);
    set_text_color(g_label_name, COLOR_WHITE);
    set_text_color(g_label_desc, COLOR_GRAY);
    set_text_color(g_label_action, COLOR_RED);

    if (g_action_pill) {
        lv_obj_set_style_bg_color(g_action_pill, lv_color_hex(0x35151A), 0);
    }

    set_text_color(g_label_status, COLOR_RED);
    lv_label_set_text(g_label_status, "ERROR");

    show_result_card_for_ms(3000);
}


void ui_hide_result_card(void)
{
    if (g_result_card) {
        lv_obj_add_flag(g_result_card, LV_OBJ_FLAG_HIDDEN);
    }

    if (g_label_status) {
        lv_label_set_text(g_label_status, "AI SCANNING");
        set_text_color(g_label_status, COLOR_CYAN);
    }
}

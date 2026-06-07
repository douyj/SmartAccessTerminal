/**
 * @file    ui/ui_main.c
 * @brief   SmartAccessTerminal 1024x600 横屏 LVGL UI
 * @details 适配 IMX6ULL 7寸 1024x600 LCD：
 *          左侧大区域模拟摄像头画面，右侧结果卡片，顶部状态栏，底部流程状态栏。
 *          通过 app_state 与 worker 后台线程解耦。
 * @author  Deng Yangjie
 * @date    2026
 */

#include "ui/ui_main.h"

#include "lvgl/lvgl.h"

#include <stdio.h>
#include <string.h>

#define LCD_W           1024
#define LCD_H           600

#define COLOR_BG        0x06111F
#define COLOR_PANEL     0x102033
#define COLOR_PANEL2    0x0B1828
#define COLOR_BLUE      0x1E90FF
#define COLOR_CYAN      0x00D1FF
#define COLOR_GREEN     0x20E070
#define COLOR_RED       0xFF4D4D
#define COLOR_YELLOW    0xFFD166
#define COLOR_WHITE     0xFFFFFF
#define COLOR_GRAY      0xA8B3C2
#define COLOR_DARK      0x07111F

/* 主区域 */
static lv_obj_t *g_video_layer = NULL;
static lv_obj_t *g_overlay_layer = NULL;

/* 顶部状态栏 */
static lv_obj_t *g_top_bar = NULL;
static lv_obj_t *g_label_time = NULL;
static lv_obj_t *g_label_wifi = NULL;
static lv_obj_t *g_label_tcp = NULL;

/* 左侧相机区域 */
static lv_obj_t *g_camera_panel = NULL;
static lv_obj_t *g_label_camera_title = NULL;
static lv_obj_t *g_label_status = NULL;
static lv_obj_t *g_label_camera_hint = NULL;

/* 右侧结果卡片 */
static lv_obj_t *g_result_card = NULL;
static lv_obj_t *g_face_thumb = NULL;
static lv_obj_t *g_face_thumb_label = NULL;

static lv_obj_t *g_text_group = NULL;
static lv_obj_t *g_label_title = NULL;
static lv_obj_t *g_label_name = NULL;
static lv_obj_t *g_label_desc = NULL;

static lv_obj_t *g_action_pill = NULL;
static lv_obj_t *g_label_action = NULL;

/* 底部状态栏 */
static lv_obj_t *g_bottom_bar = NULL;
static lv_obj_t *g_label_bottom_status = NULL;
static lv_obj_t *g_label_bottom_hint = NULL;

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


/*
 * 横屏版不真正隐藏右侧结果卡片。
 * 收到结果后显示几秒，然后恢复为 Waiting 状态。
 */
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


static void style_panel(lv_obj_t *obj, unsigned int bg, int radius)
{
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_90, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(0x18324F), 0);
    lv_obj_set_style_border_opa(obj, LV_OPA_60, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}


/**
 * @brief 背景层
 */
static void create_video_layer(lv_obj_t *scr)
{
    g_video_layer = lv_obj_create(scr);
    lv_obj_set_size(g_video_layer, LCD_W, LCD_H);
    lv_obj_set_pos(g_video_layer, 0, 0);

    lv_obj_set_style_bg_color(g_video_layer, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_border_width(g_video_layer, 0, 0);
    lv_obj_set_style_pad_all(g_video_layer, 0, 0);
    lv_obj_clear_flag(g_video_layer, LV_OBJ_FLAG_SCROLLABLE);
}


/**
 * @brief 透明悬浮层
 */
static void create_overlay_layer(lv_obj_t *scr)
{
    g_overlay_layer = lv_obj_create(scr);
    lv_obj_set_size(g_overlay_layer, LCD_W, LCD_H);
    lv_obj_set_pos(g_overlay_layer, 0, 0);

    lv_obj_set_style_bg_opa(g_overlay_layer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_overlay_layer, 0, 0);
    lv_obj_set_style_pad_all(g_overlay_layer, 0, 0);
    lv_obj_clear_flag(g_overlay_layer, LV_OBJ_FLAG_SCROLLABLE);
}


/**
 * @brief 顶部状态栏
 */
static void create_top_bar(lv_obj_t *parent)
{
    g_top_bar = lv_obj_create(parent);
    lv_obj_set_size(g_top_bar, 960, 56);
    lv_obj_align(g_top_bar, LV_ALIGN_TOP_MID, 0, 18);

    lv_obj_set_style_radius(g_top_bar, 20, 0);
    lv_obj_set_style_bg_color(g_top_bar, lv_color_hex(COLOR_DARK), 0);
    lv_obj_set_style_bg_opa(g_top_bar, LV_OPA_80, 0);
    lv_obj_set_style_border_width(g_top_bar, 0, 0);
    lv_obj_set_style_pad_all(g_top_bar, 0, 0);
    lv_obj_clear_flag(g_top_bar, LV_OBJ_FLAG_SCROLLABLE);

    g_label_time = make_label(g_top_bar,
                              "23:13",
                              28,
                              17,
                              COLOR_WHITE,
                              &lv_font_montserrat_18);

    lv_obj_t *title = make_label(g_top_bar,
                                 "Smart Access Terminal",
                                 330,
                                 15,
                                 COLOR_CYAN,
                                 &lv_font_montserrat_20);

    (void)title;

    g_label_wifi = make_label(g_top_bar,
                              "WiFi ON",
                              760,
                              19,
                              COLOR_GREEN,
                              &lv_font_montserrat_14);

    g_label_tcp = make_label(g_top_bar,
                             "TCP OFF",
                             850,
                             19,
                             COLOR_RED,
                             &lv_font_montserrat_14);
}


/**
 * @brief 左侧相机/扫描区域
 */
static void create_camera_panel(lv_obj_t *parent)
{
    g_camera_panel = lv_obj_create(parent);
    lv_obj_set_size(g_camera_panel, 640, 400);
    lv_obj_set_pos(g_camera_panel, 32, 92);
    style_panel(g_camera_panel, 0x071827, 26);

    g_label_camera_title = make_label(g_camera_panel,
                                      "LIVE CAMERA",
                                      32,
                                      28,
                                      COLOR_CYAN,
                                      &lv_font_montserrat_20);

    lv_obj_t *corner1 = lv_obj_create(g_camera_panel);
    lv_obj_set_size(corner1, 86, 4);
    lv_obj_set_pos(corner1, 32, 76);
    lv_obj_set_style_bg_color(corner1, lv_color_hex(COLOR_CYAN), 0);
    lv_obj_set_style_bg_opa(corner1, LV_OPA_80, 0);
    lv_obj_set_style_border_width(corner1, 0, 0);
    lv_obj_clear_flag(corner1, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *corner2 = lv_obj_create(g_camera_panel);
    lv_obj_set_size(corner2, 4, 86);
    lv_obj_set_pos(corner2, 32, 76);
    lv_obj_set_style_bg_color(corner2, lv_color_hex(COLOR_CYAN), 0);
    lv_obj_set_style_bg_opa(corner2, LV_OPA_80, 0);
    lv_obj_set_style_border_width(corner2, 0, 0);
    lv_obj_clear_flag(corner2, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *corner3 = lv_obj_create(g_camera_panel);
    lv_obj_set_size(corner3, 86, 4);
    lv_obj_set_pos(corner3, 520, 315);
    lv_obj_set_style_bg_color(corner3, lv_color_hex(COLOR_CYAN), 0);
    lv_obj_set_style_bg_opa(corner3, LV_OPA_80, 0);
    lv_obj_set_style_border_width(corner3, 0, 0);
    lv_obj_clear_flag(corner3, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *corner4 = lv_obj_create(g_camera_panel);
    lv_obj_set_size(corner4, 4, 86);
    lv_obj_set_pos(corner4, 602, 233);
    lv_obj_set_style_bg_color(corner4, lv_color_hex(COLOR_CYAN), 0);
    lv_obj_set_style_bg_opa(corner4, LV_OPA_80, 0);
    lv_obj_set_style_border_width(corner4, 0, 0);
    lv_obj_clear_flag(corner4, LV_OBJ_FLAG_SCROLLABLE);

    g_label_status = lv_label_create(g_camera_panel);
    lv_label_set_text(g_label_status, "AI SCANNING");
    lv_obj_set_style_text_color(g_label_status, lv_color_hex(COLOR_CYAN), 0);
    lv_obj_set_style_text_font(g_label_status, &lv_font_montserrat_24, 0);
    lv_obj_align(g_label_status, LV_ALIGN_CENTER, 0, -10);

    g_label_camera_hint = lv_label_create(g_camera_panel);
    lv_label_set_text(g_label_camera_hint, "Waiting for face snapshot...");
    lv_obj_set_style_text_color(g_label_camera_hint, lv_color_hex(COLOR_GRAY), 0);
    lv_obj_set_style_text_font(g_label_camera_hint, &lv_font_montserrat_14, 0);
    lv_obj_align(g_label_camera_hint, LV_ALIGN_CENTER, 0, 38);
}


/**
 * @brief 右侧识别结果卡片
 */
static void create_result_card(lv_obj_t *parent)
{
    g_result_card = lv_obj_create(parent);
    lv_obj_set_size(g_result_card, 304, 400);
    lv_obj_set_pos(g_result_card, 704, 92);
    style_panel(g_result_card, COLOR_PANEL, 26);

    lv_obj_t *card_title = make_label(g_result_card,
                                      "Recognition Result",
                                      28,
                                      24,
                                      COLOR_GRAY,
                                      &lv_font_montserrat_14);
    (void)card_title;

    g_face_thumb = lv_obj_create(g_result_card);
    lv_obj_set_size(g_face_thumb, 120, 120);
    lv_obj_align(g_face_thumb, LV_ALIGN_TOP_MID, 0, 58);

    lv_obj_set_style_radius(g_face_thumb, 60, 0);
    lv_obj_set_style_bg_color(g_face_thumb, lv_color_hex(COLOR_PANEL2), 0);
    lv_obj_set_style_bg_opa(g_face_thumb, LV_OPA_90, 0);
    lv_obj_set_style_border_color(g_face_thumb, lv_color_hex(COLOR_BLUE), 0);
    lv_obj_set_style_border_opa(g_face_thumb, LV_OPA_80, 0);
    lv_obj_set_style_border_width(g_face_thumb, 2, 0);
    lv_obj_set_style_pad_all(g_face_thumb, 0, 0);
    lv_obj_clear_flag(g_face_thumb, LV_OBJ_FLAG_SCROLLABLE);

    g_face_thumb_label = lv_label_create(g_face_thumb);
    lv_label_set_text(g_face_thumb_label, "FACE");
    lv_obj_set_style_text_color(g_face_thumb_label, lv_color_hex(0x315C88), 0);
    lv_obj_set_style_text_font(g_face_thumb_label, &lv_font_montserrat_18, 0);
    lv_obj_center(g_face_thumb_label);

    g_text_group = lv_obj_create(g_result_card);
    lv_obj_set_size(g_text_group, 260, 160);
    lv_obj_set_pos(g_text_group, 22, 205);

    lv_obj_set_style_bg_opa(g_text_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_text_group, 0, 0);
    lv_obj_set_style_pad_all(g_text_group, 0, 0);
    lv_obj_clear_flag(g_text_group, LV_OBJ_FLAG_SCROLLABLE);

    g_label_title = make_label(g_text_group,
                               "Waiting",
                               0,
                               0,
                               COLOR_CYAN,
                               &lv_font_montserrat_24);

    g_label_name = make_label(g_text_group,
                              "No result yet",
                              0,
                              46,
                              COLOR_WHITE,
                              &lv_font_montserrat_20);

    g_label_desc = make_label(g_text_group,
                              "Confidence --",
                              0,
                              84,
                              COLOR_GRAY,
                              &lv_font_montserrat_14);

    g_action_pill = lv_obj_create(g_text_group);
    lv_obj_set_size(g_action_pill, 150, 34);
    lv_obj_set_pos(g_action_pill, 0, 124);

    lv_obj_set_style_radius(g_action_pill, 17, 0);
    lv_obj_set_style_bg_color(g_action_pill, lv_color_hex(0x0E2A1C), 0);
    lv_obj_set_style_bg_opa(g_action_pill, LV_OPA_80, 0);
    lv_obj_set_style_border_width(g_action_pill, 0, 0);
    lv_obj_set_style_pad_all(g_action_pill, 0, 0);
    lv_obj_clear_flag(g_action_pill, LV_OBJ_FLAG_SCROLLABLE);

    g_label_action = lv_label_create(g_action_pill);
    lv_label_set_text(g_label_action, "Standby");
    lv_obj_set_style_text_color(g_label_action, lv_color_hex(COLOR_GREEN), 0);
    lv_obj_set_style_text_font(g_label_action, &lv_font_montserrat_14, 0);
    lv_obj_center(g_label_action);
}


/**
 * @brief 底部流程状态栏
 */
static void create_bottom_bar(lv_obj_t *parent)
{
    g_bottom_bar = lv_obj_create(parent);
    lv_obj_set_size(g_bottom_bar, 960, 66);
    lv_obj_align(g_bottom_bar, LV_ALIGN_BOTTOM_MID, 0, -18);

    lv_obj_set_style_radius(g_bottom_bar, 20, 0);
    lv_obj_set_style_bg_color(g_bottom_bar, lv_color_hex(COLOR_DARK), 0);
    lv_obj_set_style_bg_opa(g_bottom_bar, LV_OPA_80, 0);
    lv_obj_set_style_border_width(g_bottom_bar, 0, 0);
    lv_obj_set_style_pad_all(g_bottom_bar, 0, 0);
    lv_obj_clear_flag(g_bottom_bar, LV_OBJ_FLAG_SCROLLABLE);

    g_label_bottom_status = make_label(g_bottom_bar,
                                       "Current Status: IDLE",
                                       28,
                                       20,
                                       COLOR_WHITE,
                                       &lv_font_montserrat_18);

    g_label_bottom_hint = make_label(g_bottom_bar,
                                     "V4L2 RGB565 | TCP Upload | JSON Result | Door Control",
                                     520,
                                     22,
                                     COLOR_GRAY,
                                     &lv_font_montserrat_14);
}


void ui_main_create(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    create_video_layer(scr);
    create_overlay_layer(scr);

    create_top_bar(g_overlay_layer);
    create_camera_panel(g_overlay_layer);
    create_result_card(g_overlay_layer);
    create_bottom_bar(g_overlay_layer);

    ui_set_state(UI_STATE_IDLE);
}


void ui_set_top_info(const char *time_str, int wifi_ok, int tcp_online)
{
    if (g_label_time && time_str) {
        lv_label_set_text(g_label_time, time_str);
    }

    if (g_label_wifi) {
        lv_label_set_text(g_label_wifi, wifi_ok ? "WiFi ON" : "WiFi OFF");
        set_text_color(g_label_wifi, wifi_ok ? COLOR_GREEN : COLOR_RED);
    }

    if (g_label_tcp) {
        lv_label_set_text(g_label_tcp, tcp_online ? "TCP ON" : "TCP OFF");
        set_text_color(g_label_tcp, tcp_online ? COLOR_GREEN : COLOR_RED);
    }
}


static void set_bottom_status(const char *text, unsigned int color)
{
    if (g_label_bottom_status) {
        lv_label_set_text(g_label_bottom_status, text);
        set_text_color(g_label_bottom_status, color);
    }
}


void ui_set_state(UiState state)
{
    if (!g_label_status) {
        return;
    }

    switch (state) {
    case UI_STATE_IDLE:
        lv_label_set_text(g_label_status, "AI SCANNING");
        set_text_color(g_label_status, COLOR_CYAN);

        if (g_label_camera_hint) {
            lv_label_set_text(g_label_camera_hint, "Waiting for face snapshot...");
            set_text_color(g_label_camera_hint, COLOR_GRAY);
        }

        set_bottom_status("Current Status: IDLE", COLOR_WHITE);
        break;

    case UI_STATE_CAPTURING:
        lv_label_set_text(g_label_status, "CAPTURING");
        set_text_color(g_label_status, COLOR_CYAN);

        if (g_label_camera_hint) {
            lv_label_set_text(g_label_camera_hint, "Camera is capturing RGB565 frame");
            set_text_color(g_label_camera_hint, COLOR_CYAN);
        }

        set_bottom_status("Current Status: CAPTURING", COLOR_CYAN);
        break;

    case UI_STATE_UPLOADING:
        lv_label_set_text(g_label_status, "UPLOADING");
        set_text_color(g_label_status, COLOR_BLUE);

        if (g_label_camera_hint) {
            lv_label_set_text(g_label_camera_hint, "Uploading snapshot to Mac Qt server");
            set_text_color(g_label_camera_hint, COLOR_BLUE);
        }

        set_bottom_status("Current Status: UPLOADING", COLOR_BLUE);
        break;

    case UI_STATE_VERIFYING:
        lv_label_set_text(g_label_status, "VERIFYING");
        set_text_color(g_label_status, COLOR_YELLOW);

        if (g_label_camera_hint) {
            lv_label_set_text(g_label_camera_hint, "Waiting for JSON recognition result");
            set_text_color(g_label_camera_hint, COLOR_YELLOW);
        }

        set_bottom_status("Current Status: VERIFYING", COLOR_YELLOW);
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

    lv_label_set_text(g_label_title, "ACCESS GRANTED");

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

    if (g_label_status) {
        lv_label_set_text(g_label_status, "ACCESS GRANTED");
        set_text_color(g_label_status, COLOR_GREEN);
    }

    if (g_label_camera_hint) {
        lv_label_set_text(g_label_camera_hint, "Identity verified, door is open");
        set_text_color(g_label_camera_hint, COLOR_GREEN);
    }

    set_bottom_status("Current Status: ACCESS GRANTED", COLOR_GREEN);

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

    lv_label_set_text(g_label_title, "ACCESS DENIED");

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

    if (g_label_status) {
        lv_label_set_text(g_label_status, "ACCESS DENIED");
        set_text_color(g_label_status, COLOR_RED);
    }

    if (g_label_camera_hint) {
        lv_label_set_text(g_label_camera_hint, "Access rejected, alarm triggered");
        set_text_color(g_label_camera_hint, COLOR_RED);
    }

    set_bottom_status("Current Status: ACCESS DENIED", COLOR_RED);

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

    lv_label_set_text(g_label_title, "NO FACE");
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

    if (g_label_status) {
        lv_label_set_text(g_label_status, "NO FACE");
        set_text_color(g_label_status, COLOR_YELLOW);
    }

    if (g_label_camera_hint) {
        lv_label_set_text(g_label_camera_hint, "No valid face detected");
        set_text_color(g_label_camera_hint, COLOR_YELLOW);
    }

    set_bottom_status("Current Status: NO FACE", COLOR_YELLOW);

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

    lv_label_set_text(g_label_title, "SYSTEM ERROR");
    lv_label_set_text(g_label_name, msg ? msg : "Unknown error");
    lv_label_set_text(g_label_desc, "Check network or device");
    lv_label_set_text(g_label_action, "Closed");

    set_text_color(g_label_title, COLOR_RED);
    set_text_color(g_label_name, COLOR_WHITE);
    set_text_color(g_label_desc, COLOR_GRAY);
    set_text_color(g_label_action, COLOR_RED);

    if (g_action_pill) {
        lv_obj_set_style_bg_color(g_action_pill, lv_color_hex(0x35151A), 0);
    }

    if (g_label_status) {
        lv_label_set_text(g_label_status, "ERROR");
        set_text_color(g_label_status, COLOR_RED);
    }

    if (g_label_camera_hint) {
        lv_label_set_text(g_label_camera_hint, "System error, please check logs");
        set_text_color(g_label_camera_hint, COLOR_RED);
    }

    set_bottom_status("Current Status: ERROR", COLOR_RED);

    show_result_card_for_ms(3000);
}


void ui_hide_result_card(void)
{
    /*
     * 横屏版不隐藏右侧面板，只恢复为 Waiting 状态。
     */
    if (g_face_thumb_label) {
        lv_label_set_text(g_face_thumb_label, "FACE");
        set_text_color(g_face_thumb_label, COLOR_BLUE);
    }

    if (g_face_thumb) {
        lv_obj_set_style_border_color(g_face_thumb, lv_color_hex(COLOR_BLUE), 0);
    }

    if (g_label_title) {
        lv_label_set_text(g_label_title, "Waiting");
        set_text_color(g_label_title, COLOR_CYAN);
    }

    if (g_label_name) {
        lv_label_set_text(g_label_name, "No result yet");
        set_text_color(g_label_name, COLOR_WHITE);
    }

    if (g_label_desc) {
        lv_label_set_text(g_label_desc, "Confidence --");
        set_text_color(g_label_desc, COLOR_GRAY);
    }

    if (g_label_action) {
        lv_label_set_text(g_label_action, "Standby");
        set_text_color(g_label_action, COLOR_GREEN);
    }

    if (g_action_pill) {
        lv_obj_set_style_bg_color(g_action_pill, lv_color_hex(0x0E2A1C), 0);
    }

    if (g_label_status) {
        lv_label_set_text(g_label_status, "AI SCANNING");
        set_text_color(g_label_status, COLOR_CYAN);
    }

    if (g_label_camera_hint) {
        lv_label_set_text(g_label_camera_hint, "Waiting for face snapshot...");
        set_text_color(g_label_camera_hint, COLOR_GRAY);
    }

    set_bottom_status("Current Status: IDLE", COLOR_WHITE);
}


static UiState app_status_to_ui_state(AppStatus status)
{
    switch (status) {
    case APP_STATUS_IDLE:
        return UI_STATE_IDLE;
    case APP_STATUS_CAPTURING:
        return UI_STATE_CAPTURING;
    case APP_STATUS_UPLOADING:
        return UI_STATE_UPLOADING;
    case APP_STATUS_VERIFYING:
        return UI_STATE_VERIFYING;
    case APP_STATUS_SUCCESS:
        return UI_STATE_SUCCESS;
    case APP_STATUS_DENY:
        return UI_STATE_DENY;
    case APP_STATUS_NO_FACE:
        return UI_STATE_NO_FACE;
    case APP_STATUS_ERROR:
        return UI_STATE_ERROR;
    default:
        return UI_STATE_IDLE;
    }
}


void ui_update_from_app_state(void)
{
    AppStateSnapshot snapshot;

    app_state_get_snapshot(&snapshot);

    /*
     * 顶部状态栏每次刷新。
     * 时间暂时写死，后续可接 time(NULL) 或 RTC。
     */
    ui_set_top_info("23:13", snapshot.wifi_ok, snapshot.tcp_online);

    /*
     * 有新识别结果时，优先刷新右侧结果卡片。
     */
    if (snapshot.result_updated) {
        switch (snapshot.result) {
        case APP_RESULT_ALLOW:
            ui_show_success(snapshot.name, snapshot.confidence, 3);
            break;

        case APP_RESULT_DENY:
            ui_show_deny(snapshot.name, snapshot.confidence);
            break;

        case APP_RESULT_NO_FACE:
            ui_show_no_face();
            break;

        case APP_RESULT_ERROR:
            ui_show_error(snapshot.message);
            break;

        case APP_RESULT_NONE:
        default:
            break;
        }

        app_state_clear_result_updated();
        return;
    }

    /*
     * 没有新结果时，同步过程状态。
     */
    ui_set_state(app_status_to_ui_state(snapshot.status));
}

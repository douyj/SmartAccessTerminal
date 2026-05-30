#include "ui/ui_main.h"

#include "lvgl/lvgl.h"

#include <stdio.h>


static lv_obj_t *label_device_id = NULL;
static lv_obj_t *label_server = NULL;
static lv_obj_t *label_camera = NULL;
static lv_obj_t *label_status = NULL;
static lv_obj_t *label_result = NULL;
static lv_obj_t *label_name = NULL;
static lv_obj_t *label_confidence = NULL;
static lv_obj_t *label_action = NULL;
static lv_obj_t *label_error = NULL;


static lv_obj_t *create_label(lv_obj_t *parent, const char *text, int x, int y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    return label;
}


void ui_main_create(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Smart Access Terminal");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    lv_obj_t *card = lv_obj_create(scr);
    lv_obj_set_size(card, 740, 380);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1E2A36), 0);
    lv_obj_set_style_border_width(card, 0, 0);

    lv_obj_t *info_title = create_label(card, "Device Info", 24, 20);
    lv_obj_set_style_text_color(info_title, lv_color_hex(0x00D1FF), 0);
    lv_obj_set_style_text_font(info_title, &lv_font_montserrat_18, 0);

    label_device_id = create_label(card, "Device ID: -", 24, 60);
    label_server = create_label(card, "Server: -", 24, 95);
    label_camera = create_label(card, "Camera: -", 24, 130);

    lv_obj_t *status_title = create_label(card, "Status", 24, 185);
    lv_obj_set_style_text_color(status_title, lv_color_hex(0x00D1FF), 0);
    lv_obj_set_style_text_font(status_title, &lv_font_montserrat_18, 0);

    label_status = create_label(card, "Waiting", 24, 225);
    lv_obj_set_style_text_color(label_status, lv_color_hex(0xFFD166), 0);
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_22, 0);

    lv_obj_t *result_title = create_label(card, "Last Result", 400, 20);
    lv_obj_set_style_text_color(result_title, lv_color_hex(0x00D1FF), 0);
    lv_obj_set_style_text_font(result_title, &lv_font_montserrat_18, 0);

    label_result = create_label(card, "Result: -", 400, 60);
    label_name = create_label(card, "Name: -", 400, 95);
    label_confidence = create_label(card, "Confidence: -", 400, 130);
    label_action = create_label(card, "Action: -", 400, 165);

    label_error = create_label(card, "", 400, 240);
    lv_obj_set_style_text_color(label_error, lv_color_hex(0xFF4D4D), 0);

    lv_obj_set_style_text_color(label_device_id, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_color(label_server, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_color(label_camera, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_color(label_result, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_color(label_name, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_color(label_confidence, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_color(label_action, lv_color_hex(0xFFFFFF), 0);
}


void ui_set_device_info(const char *device_id,
                        const char *server,
                        const char *camera)
{
    if (label_device_id) {
        lv_label_set_text_fmt(label_device_id, "Device ID: %s", device_id);
    }

    if (label_server) {
        lv_label_set_text_fmt(label_server, "Server: %s", server);
    }

    if (label_camera) {
        lv_label_set_text_fmt(label_camera, "Camera: %s", camera);
    }
}


void ui_set_status(const char *status)
{
    if (label_status == NULL) {
        return;
    }

    lv_label_set_text(label_status, status);

    if (status == NULL) {
        return;
    }

    if (strcmp(status, "ALLOW") == 0) {
        lv_obj_set_style_text_color(label_status, lv_color_hex(0x06D6A0), 0);
    } else if (strcmp(status, "DENY") == 0) {
        lv_obj_set_style_text_color(label_status, lv_color_hex(0xFF4D4D), 0);
    } else if (strcmp(status, "NO_FACE") == 0) {
        lv_obj_set_style_text_color(label_status, lv_color_hex(0xFFD166), 0);
    } else if (strcmp(status, "ERROR") == 0) {
        lv_obj_set_style_text_color(label_status, lv_color_hex(0xFF4D4D), 0);
    } else {
        lv_obj_set_style_text_color(label_status, lv_color_hex(0x00D1FF), 0);
    }
}


void ui_show_result(const char *result,
                    const char *name,
                    float confidence,
                    const char *action)
{
    if (label_result) {
        lv_label_set_text_fmt(label_result, "Result: %s", result);
    }

    if (label_name) {
        lv_label_set_text_fmt(label_name, "Name: %s", name);
    }

    if (label_confidence) {
        lv_label_set_text_fmt(label_confidence, "Confidence: %.2f", confidence);
    }

    if (label_action) {
        lv_label_set_text_fmt(label_action, "Action: %s", action);
    }

    if (label_error) {
        lv_label_set_text(label_error, "");
    }
}


void ui_show_error(const char *error_msg)
{
    ui_set_status("ERROR");

    if (label_error) {
        lv_label_set_text_fmt(label_error, "Error: %s", error_msg);
    }
}

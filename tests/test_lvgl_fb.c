#include "lvgl/lvgl.h"
#include "lv_drivers/display/fbdev.h"

#include <unistd.h>
#include <stdio.h>

static void create_demo_ui(void)
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x07111C), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Smart Access Terminal");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00D1FF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t *status = lv_label_create(scr);
    lv_label_set_text(status, "LVGL Framebuffer OK");
    lv_obj_set_style_text_color(status, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_20, 0);
    lv_obj_align(status, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 220, 64);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 80);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "IMX6ULL LCD");
    lv_obj_center(btn_label);
}

int main(void)
{
    printf("test_lvgl_fb start\n");

    lv_init();

    /*
     * 初始化 Linux framebuffer。
     * 默认使用 /dev/fb0。
     */
    fbdev_init();

    static lv_disp_draw_buf_t draw_buf;

    /*
     * 先按 800x480 准备 buffer。
     * 如果你的屏幕是 480x272，也没关系，后面改 hor_res/ver_res。
     */
    static lv_color_t buf1[1024 * 80];
    static lv_color_t buf2[1024 * 80];

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, 1024 * 80);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = fbdev_flush;

    /*
     * 注意：
     * 如果你的开发板屏幕是 800x480，就保持下面这样。
     * 如果是 480x272，要改成：
     * disp_drv.hor_res = 480;
     * disp_drv.ver_res = 272;
     */
    disp_drv.hor_res = 1024;
    disp_drv.ver_res = 600;

    lv_disp_drv_register(&disp_drv);

    create_demo_ui();

    while (1) {
        lv_tick_inc(5);
        lv_timer_handler();
        usleep(5 * 1000);
    }

    return 0;
}

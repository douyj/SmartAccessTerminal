#include "lvgl/lvgl.h"
#include "ui/ui_main.h"

#include <unistd.h>
#include <stdio.h>

#if USE_SDL
#include "sdl/sdl.h"
#endif


int main(void)
{
    lv_init();

#if USE_SDL
    sdl_init();

    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf1[800 * 80];

    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, 800 * 80);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.hor_res = 800;
    disp_drv.ver_res = 480;

    lv_disp_drv_register(&disp_drv);
#else
    printf("USE_SDL is not enabled\n");
    return -1;
#endif

    ui_main_create();

    ui_set_device_info(
        "imx6ull_001",
        "192.168.43.30:9000",
        "/dev/video1"
    );

    ui_set_status("WAITING");

    int tick = 0;

    while (1) {
        lv_timer_handler();
        usleep(5 * 1000);

        tick++;

        if (tick == 200) {
            ui_set_status("CAPTURING");
        } else if (tick == 400) {
            ui_set_status("UPLOADING");
        } else if (tick == 600) {
            ui_set_status("ALLOW");
            ui_show_result("allow", "DengYangjie", 0.96f, "open_door");
        } else if (tick == 1000) {
            ui_set_status("DENY");
            ui_show_result("deny", "Unknown", 0.42f, "alarm_beep");
        } else if (tick == 1400) {
            ui_set_status("NO_FACE");
            ui_show_result("no_face", "None", 0.00f, "none");
        } else if (tick == 1800) {
            ui_show_error("connect server failed");
        } else if (tick > 2200) {
            tick = 0;
            ui_set_status("WAITING");
            ui_show_result("-", "-", 0.0f, "-");
        }
    }

    return 0;
}

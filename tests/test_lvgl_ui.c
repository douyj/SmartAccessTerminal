#include "lvgl/lvgl.h"
#include "ui/ui_main.h"

#include <stdio.h>
#include <unistd.h>

#if USE_SDL
#include "sdl/sdl.h"
#endif

int main(void)
{
    lv_init();

#if USE_SDL
    sdl_init();

    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf1[480 * 80];

    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, 480 * 80);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.hor_res = 480;
    disp_drv.ver_res = 800;

    lv_disp_drv_register(&disp_drv);
#else
    printf("USE_SDL is not enabled\n");
    return -1;
#endif

    ui_main_create();
    ui_set_top_info("23:13", 1, 1);

    int tick = 0;

    while (1) {
    #if USE_SDL
        if (sdl_quit_qry) {
            break;
        }
    #endif

        lv_tick_inc(5);
        lv_timer_handler();
        usleep(5 * 1000);

        tick++;

        if (tick == 200) {
            ui_set_state(UI_STATE_CAPTURING);
        } else if (tick == 400) {
            ui_set_state(UI_STATE_UPLOADING);
        } else if (tick == 600) {
            ui_set_state(UI_STATE_VERIFYING);
        } else if (tick == 850) {
            ui_show_success("Deng Yangjie", 0.96f, 3);
        } else if (tick == 1500) {
            ui_show_deny("Unknown", 0.31f);
        } else if (tick == 2150) {
            ui_show_no_face();
        } else if (tick == 2800) {
            ui_show_error("TCP disconnected");
        } else if (tick > 3500) {
            tick = 0;
            ui_set_state(UI_STATE_IDLE);
        }
    }

    return 0;
}

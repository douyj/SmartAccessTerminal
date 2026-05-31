#include "lvgl/lvgl.h"
#include "ui/ui_main.h"
#include "app/app_state.h"

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#if USE_SDL
#include "sdl/sdl.h"
#endif

static int g_running = 1;

static void *mock_network_thread(void *arg)
{
    (void)arg;

    while (g_running) {
        sleep(1);
        app_state_set_wifi_ok(1);
        app_state_set_tcp_online(1);
        app_state_set_status(APP_STATUS_CAPTURING);

        sleep(1);
        app_state_set_status(APP_STATUS_UPLOADING);

        sleep(1);
        app_state_set_status(APP_STATUS_VERIFYING);

        sleep(1);
        app_state_set_result_allow(
            "Deng Yangjie",
            0.96f,
            "open_door",
            "access granted"
        );

        sleep(5);
        app_state_set_status(APP_STATUS_VERIFYING);

        sleep(1);
        app_state_set_result_deny(
            "Unknown",
            0.31f,
            "alarm_beep",
            "access denied"
        );

        sleep(5);
        app_state_set_status(APP_STATUS_VERIFYING);

        sleep(1);
        app_state_set_result_no_face("no face detected");

        sleep(5);
        app_state_set_status(APP_STATUS_VERIFYING);

        sleep(1);
        app_state_set_error("TCP disconnected");

        sleep(5);
        app_state_set_status(APP_STATUS_IDLE);
    }

    return NULL;
}

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

    app_state_init();

    ui_main_create();
    ui_set_top_info("23:13", 1, 1);

    pthread_t tid;
    pthread_create(&tid, NULL, mock_network_thread, NULL);

    while (1) {
#if USE_SDL
        if (sdl_quit_qry) {
            break;
        }
#endif

        lv_tick_inc(5);

        ui_update_from_app_state();

        lv_timer_handler();
        usleep(5 * 1000);
    }

    g_running = 0;
    pthread_join(tid, NULL);

    return 0;
}

#include "ui/ui_main.h"
#include "app/app_state.h"

#include "lvgl/lvgl.h"
#include "lv_drivers/sdl/sdl.h"

#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

static int g_running = 1;

static void *mock_worker_thread(void *arg)
{
    (void)arg;

    sleep(1);

    while (g_running) {
        app_state_set_wifi_ok(1);
        app_state_set_tcp_online(0);
        app_state_set_status(APP_STATUS_IDLE);
        sleep(2);

        app_state_set_status(APP_STATUS_CAPTURING);
        sleep(2);

        app_state_set_tcp_online(1);
        app_state_set_status(APP_STATUS_UPLOADING);
        sleep(2);

        app_state_set_status(APP_STATUS_VERIFYING);
        sleep(2);

        app_state_set_result_allow("DengYangjie",
                                   0.96f,
                                   "open_door",
                                   "face ok");
        sleep(4);

        app_state_set_status(APP_STATUS_IDLE);
        sleep(2);

        app_state_set_status(APP_STATUS_CAPTURING);
        sleep(2);

        app_state_set_tcp_online(1);
        app_state_set_status(APP_STATUS_UPLOADING);
        sleep(2);

        app_state_set_status(APP_STATUS_VERIFYING);
        sleep(2);

        app_state_set_result_deny("Unknown",
                                  0.30f,
                                  "alarm_beep",
                                  "access denied");
        sleep(4);

        app_state_set_status(APP_STATUS_IDLE);
        sleep(2);

        app_state_set_status(APP_STATUS_CAPTURING);
        sleep(2);

        app_state_set_status(APP_STATUS_VERIFYING);
        sleep(2);

        app_state_set_result_no_face("no face detected");
        sleep(4);

        app_state_set_status(APP_STATUS_IDLE);
        sleep(2);
    }

    return NULL;
}


static void hal_init(void)
{
    lv_init();

    sdl_init();

    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf1[480 * 100];
    static lv_color_t buf2[480 * 100];

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, 480 * 100);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.hor_res = 480;
    disp_drv.ver_res = 800;

    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t mouse_drv;
    lv_indev_drv_init(&mouse_drv);

    mouse_drv.type = LV_INDEV_TYPE_POINTER;
    mouse_drv.read_cb = sdl_mouse_read;

    lv_indev_drv_register(&mouse_drv);

    static lv_indev_drv_t keyboard_drv;
    lv_indev_drv_init(&keyboard_drv);

    keyboard_drv.type = LV_INDEV_TYPE_KEYPAD;
    keyboard_drv.read_cb = sdl_keyboard_read;

    lv_indev_drv_register(&keyboard_drv);
}


int main(void)
{
    pthread_t tid;

    printf("test_app_integrated_ui start\n");

    app_state_init();
    app_state_set_wifi_ok(1);
    app_state_set_tcp_online(0);
    app_state_set_status(APP_STATUS_IDLE);

    hal_init();
    ui_main_create();

    pthread_create(&tid, NULL, mock_worker_thread, NULL);

    while (1) {
        lv_tick_inc(5);

        ui_update_from_app_state();

        lv_timer_handler();

        usleep(5 * 1000);
    }

    g_running = 0;
    pthread_join(tid, NULL);

    return 0;
}

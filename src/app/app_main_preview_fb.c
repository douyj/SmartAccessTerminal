#include "app/app_main.h"
#include "app/app_state.h"

#include "ui/ui_main.h"

#include "camera/camera_preview.h"
#include "common/log.h"
#include "config/device_config.h"

#include "lvgl/lvgl.h"
#include "lv_drivers/display/fbdev.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#define LCD_W 1024
#define LCD_H 600

static volatile int g_app_running = 1;

static void print_usage(const char *prog)
{
    printf("Usage:\n");
    printf("  %s <config_path>\n", prog);
    printf("\nExample:\n");
    printf("  %s ./device_config.json\n", prog);
}

static void signal_handler(int signo)
{
    (void)signo;
    g_app_running = 0;
}

static void lvgl_fb_init(void)
{
    lv_init();

    fbdev_init();

    static lv_disp_draw_buf_t draw_buf;

    static lv_color_t buf1[1024 * 80];
    static lv_color_t buf2[1024 * 80];

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, 1024 * 80);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res = LCD_W;
    disp_drv.ver_res = LCD_H;

    lv_disp_drv_register(&disp_drv);
}

int app_main(int argc, char *argv[])
{
    if (argc != 2) {
        print_usage(argv[0]);
        return -1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    const char *config_path = argv[1];

    log_set_level(LOG_LEVEL_INFO);
    log_set_show_detail(0);

    DeviceConfig config;

    if (device_config_load(config_path, &config) < 0) {
        LOG_ERROR("load config failed: %s", config_path);
        return -1;
    }

    device_config_print(&config);

    app_state_init();
    app_state_set_wifi_ok(1);
    app_state_set_tcp_online(0);
    app_state_set_status(APP_STATUS_IDLE);

    lvgl_fb_init();
    ui_main_create();

    /*
     * 第21关建议先用 320x240 预览。
     * 如果你想用配置文件里的 640x480，可以改成：
     * config.image_width, config.image_height
     */
    int preview_w = 320;
    int preview_h = 240;

    if (camera_preview_start(config.camera_dev, preview_w, preview_h) < 0) {
        LOG_ERROR("camera preview start failed");
        app_state_set_error("camera preview start failed");
    } else {
        LOG_INFO("camera preview started: %dx%d", preview_w, preview_h);
        app_state_set_status(APP_STATUS_CAPTURING);
    }

    int frame_info_w = 0;
    int frame_info_h = 0;
    int frame_size = 0;

    /*
     * 等摄像头线程先采几帧。
     */
    usleep(300 * 1000);

    camera_preview_get_info(&frame_info_w, &frame_info_h, &frame_size);

    if (frame_size <= 0) {
        frame_size = preview_w * preview_h * 2;
    }

    unsigned char *frame_buf = (unsigned char *)malloc(frame_size);
    if (!frame_buf) {
        LOG_ERROR("malloc frame_buf failed");
        camera_preview_stop();
        return -1;
    }

    LOG_INFO("LVGL camera preview loop started");

    int frame_counter = 0;

    while (g_app_running) {
        lv_tick_inc(5);

        ui_update_from_app_state();

        /*
         * 不要每 5ms 都取图，先 50ms 一次，大约 20 FPS。
         */
        frame_counter++;
        if (frame_counter >= 10) {
            frame_counter = 0;

            int w = 0;
            int h = 0;
            int size = 0;

            if (camera_preview_get_frame(frame_buf,
                                         frame_size,
                                         &w,
                                         &h,
                                         &size) == 0) {
                ui_update_camera_frame(frame_buf, w, h, size);
            }
        }

        lv_timer_handler();

        usleep(5 * 1000);
    }

    LOG_INFO("main loop exit");

    free(frame_buf);
    camera_preview_stop();

    LOG_INFO("app exit");
    return 0;
}

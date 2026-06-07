#include "app/app_main.h"
#include "app/app_state.h"
#include "app/app_worker.h"

#include "ui/ui_main.h"

#include "common/log.h"
#include "bsp/door.h"
#include "bsp/alarm.h"
#include "config/device_config.h"
#include "storage/snapshot_storage.h"
#include "camera/camera.h"

#include "lvgl/lvgl.h"
#include "lv_drivers/display/fbdev.h"

#include <stdio.h>
#include <unistd.h>
#include <signal.h>

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

static int app_init_all(const DeviceConfig *config)
{
    log_set_level(log_level_from_string(config->log_level));
    log_set_show_detail(config->log_detail);

    app_state_init();
    app_state_set_wifi_ok(1);
    app_state_set_tcp_online(0);
    app_state_set_status(APP_STATUS_IDLE);

    if (door_init() < 0) {
        LOG_ERROR("door init failed");
        app_state_set_error("door init failed");
        return -1;
    }

    if (alarm_init() < 0) {
        LOG_ERROR("alarm init failed");
        app_state_set_error("alarm init failed");
        return -1;
    }

    if (snapshot_storage_init(config->snapshot_dir) < 0) {
        LOG_ERROR("snapshot storage init failed");
        app_state_set_error("snapshot storage init failed");
        return -1;
    }

    if (camera_init(config->camera_dev,
                    config->image_width,
                    config->image_height) < 0) {
        LOG_ERROR("camera init failed");
        app_state_set_error("camera init failed");
        return -1;
    }

    LOG_INFO("app init success");
    return 0;
}

static void app_deinit_all(void)
{
    app_worker_stop();

    camera_deinit();

    door_deinit();
    alarm_deinit();

    LOG_INFO("app deinit done");
}

static void lvgl_fb_init(void)
{
    lv_init();

    fbdev_init();

    static lv_disp_draw_buf_t draw_buf;

    /*
     * 你的开发板 LCD 是 1024x600，16bpp。
     * 这里用 80 行缓存，够用且不太占内存。
     */
    static lv_color_t buf1[1024 * 80];
    static lv_color_t buf2[1024 * 80];

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, 1024 * 80);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res = 1024;
    disp_drv.ver_res = 600;

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

    if (app_init_all(&config) < 0) {
        LOG_ERROR("app init failed");
        app_deinit_all();
        return -1;
    }

    lvgl_fb_init();
    ui_main_create();

    if (app_worker_start(&config) < 0) {
        LOG_ERROR("app worker start failed");
        app_deinit_all();
        return -1;
    }

    LOG_INFO("LVGL framebuffer main loop started");

    while (g_app_running) {
        lv_tick_inc(5);

        ui_update_from_app_state();

        lv_timer_handler();

        usleep(5 * 1000);
    }

    LOG_INFO("main loop exit");

    app_deinit_all();

    LOG_INFO("app exit");
    return 0;
}

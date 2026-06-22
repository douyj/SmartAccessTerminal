#include "app/app_main.h"
#include "app/app_state.h"
#include "app/app_worker.h"
#include "app/app_trigger.h"

#include "ui/ui_main.h"

#include "camera/camera_preview.h"
#include "common/log.h"
#include "bsp/door.h"
#include "bsp/alarm.h"
#include "config/device_config.h"
#include "storage/snapshot_storage.h"

#include "lvgl/lvgl.h"
#include "lv_drivers/display/fbdev.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>

#define LCD_W 1024
#define LCD_H 600

static volatile int g_app_running = 1;

static pthread_t g_input_thread;
static int g_input_thread_created = 0;


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

    /*
     * 唤醒等待触发的 worker，方便退出。
     */
    app_trigger_stop();
}


static void *trigger_input_thread_func(void *arg)
{
    (void)arg;

    LOG_INFO("[TRIGGER] input thread started");
    LOG_INFO("[TRIGGER] press ENTER to trigger recognition");

    while (g_app_running) {
        int ch = getchar();

        if (ch == '\n') {
            LOG_INFO("[TRIGGER] ENTER pressed");
            app_trigger_request();
        }

        if (ch == EOF) {
            usleep(100 * 1000);
        }
    }

    LOG_INFO("[TRIGGER] input thread stopped");
    return NULL;
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


static int app_init_basic(const DeviceConfig *config)
{
    log_set_level(log_level_from_string(config->log_level));
    log_set_show_detail(config->log_detail);

    app_state_init();
    app_state_set_wifi_ok(1);
    app_state_set_tcp_online(0);
    app_state_set_status(APP_STATUS_IDLE);

    app_trigger_init();

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

    return 0;
}


static void app_deinit_basic(void)
{
    /*
     * 先停止触发系统，唤醒 worker。
     */
    app_trigger_stop();

    /*
     * 再停 worker 和 camera preview。
     */
    app_worker_stop();
    camera_preview_stop();

    door_deinit();
    alarm_deinit();

    LOG_INFO("app deinit done");
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

    if (app_init_basic(&config) < 0) {
        LOG_ERROR("app init failed");
        app_deinit_basic();
        return -1;
    }

    lvgl_fb_init();
    ui_main_create();

    /*
     * 实时预览分辨率。
     * 当前先用 320x240，IMX6ULL 更稳。
     * 如果后续性能足够，可以改为：
     * int preview_w = config.image_width;
     * int preview_h = config.image_height;
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

    /*
     * 启动新版 worker：
     * 从 camera_preview latest_frame 抽帧上传。
     * 第24关后：worker 会等待 trigger，不会自动每5秒上传。
     */
    if (app_worker_start_from_preview(&config) < 0) {
        LOG_ERROR("preview worker start failed");
        app_state_set_error("preview worker start failed");
    }

    /*
     * 启动终端输入线程：
     * 按 Enter -> app_trigger_request()
     */
    if (pthread_create(&g_input_thread, NULL, trigger_input_thread_func, NULL) == 0) {
        g_input_thread_created = 1;

        /*
         * 输入线程里 getchar 可能阻塞。
         * 这里 detach，主线程退出时不强制 join。
         */
        pthread_detach(g_input_thread);
    } else {
        LOG_ERROR("trigger input thread create failed");
    }

    int frame_info_w = 0;
    int frame_info_h = 0;
    int frame_size = 0;

    /*
     * 等摄像头 preview 线程先采几帧。
     */
    usleep(300 * 1000);

    camera_preview_get_info(&frame_info_w, &frame_info_h, &frame_size);

    if (frame_size <= 0) {
        frame_size = preview_w * preview_h * 2;
    }

    unsigned char *frame_buf = (unsigned char *)malloc(frame_size);
    if (!frame_buf) {
        LOG_ERROR("malloc frame_buf failed");
        app_deinit_basic();
        return -1;
    }

    LOG_INFO("smart_access_terminal_live_fb main loop started");

    int frame_counter = 0;

    while (g_app_running) {
        lv_tick_inc(5);

        /*
         * 第24关：
         * 更新 trigger 状态。
         * 如果当前处于 COOLDOWN，时间到了会自动恢复 IDLE。
         */
        app_trigger_update();

        /*
         * 从 app_state 刷新 UI：
         * 包括顶部 TCP 状态、右侧识别结果、底部状态。
         */
        ui_update_from_app_state();

        /*
         * 每 50ms 刷新一次 LCD 摄像头图像，约 20 FPS。
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
    frame_buf = NULL;

    app_deinit_basic();

    LOG_INFO("app exit");
    return 0;
}
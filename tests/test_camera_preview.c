#include "camera/camera_preview.h"
#include "common/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


static int save_rgb565_raw(const char *path, const unsigned char *data, int size)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        printf("open raw file failed: %s\n", path);
        return -1;
    }

    fwrite(data, 1, size, fp);
    fclose(fp);

    return 0;
}


static int save_rgb565_as_ppm(const char *path,
                              const unsigned char *data,
                              int width,
                              int height)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        printf("open ppm file failed: %s\n", path);
        return -1;
    }

    fprintf(fp, "P6\n%d %d\n255\n", width, height);

    for (int i = 0; i < width * height; i++) {
        unsigned short pixel = data[i * 2] | (data[i * 2 + 1] << 8);

        unsigned char r = (pixel >> 11) & 0x1F;
        unsigned char g = (pixel >> 5) & 0x3F;
        unsigned char b = pixel & 0x1F;

        r = (r << 3) | (r >> 2);
        g = (g << 2) | (g >> 4);
        b = (b << 3) | (b >> 2);

        fputc(r, fp);
        fputc(g, fp);
        fputc(b, fp);
    }

    fclose(fp);
    return 0;
}


int main(int argc, char *argv[])
{
    const char *dev = "/dev/video1";
    int width = 640;
    int height = 480;

    if (argc >= 2) {
        dev = argv[1];
    }

    if (argc >= 4) {
        width = atoi(argv[2]);
        height = atoi(argv[3]);
    }

    log_set_level(LOG_LEVEL_INFO);
    log_set_show_detail(0);

    printf("========== Camera Preview Test ==========\n");
    printf("device : %s\n", dev);
    printf("size   : %dx%d\n", width, height);
    printf("=========================================\n");

    if (camera_preview_start(dev, width, height) < 0) {
        printf("camera_preview_start failed\n");
        return -1;
    }

    int real_w = 0;
    int real_h = 0;
    int frame_size = 0;

    sleep(1);

    camera_preview_get_info(&real_w, &real_h, &frame_size);

    printf("real size  : %dx%d\n", real_w, real_h);
    printf("frame size : %d\n", frame_size);

    unsigned char *frame_buf = (unsigned char *)malloc(frame_size);
    if (!frame_buf) {
        printf("malloc frame_buf failed\n");
        camera_preview_stop();
        return -1;
    }

    int count = 0;

    while (1) {
        sleep(1);
        count++;

        printf("[PREVIEW TEST] fps=%d\n", camera_preview_get_fps());

        if (count % 3 == 0) {
            int w = 0;
            int h = 0;
            int size = 0;

            if (camera_preview_get_frame(frame_buf,
                                         frame_size,
                                         &w,
                                         &h,
                                         &size) == 0) {
                printf("[PREVIEW TEST] save frame: %dx%d, size=%d\n",
                       w,
                       h,
                       size);

                save_rgb565_raw("preview_frame.rgb565", frame_buf, size);
                save_rgb565_as_ppm("preview_frame.ppm", frame_buf, w, h);

                printf("[PREVIEW TEST] saved preview_frame.rgb565 and preview_frame.ppm\n");
            } else {
                printf("[PREVIEW TEST] get frame failed\n");
            }
        }
    }

    free(frame_buf);
    camera_preview_stop();

    return 0;
}

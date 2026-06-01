#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <linux/videodev2.h>

#define BUFFER_COUNT 4

typedef struct {
    void *start;
    size_t length;
} V4L2Buffer;

static int xioctl(int fd, unsigned long request, void *arg)
{
    int ret;

    do {
        ret = ioctl(fd, request, arg);
    } while (ret == -1 && errno == EINTR);

    return ret;
}

static void print_fourcc(unsigned int pixelformat)
{
    char fourcc[5];

    fourcc[0] = pixelformat & 0xFF;
    fourcc[1] = (pixelformat >> 8) & 0xFF;
    fourcc[2] = (pixelformat >> 16) & 0xFF;
    fourcc[3] = (pixelformat >> 24) & 0xFF;
    fourcc[4] = '\0';

    printf("%s", fourcc);
}

static void enum_formats(int fd)
{
    struct v4l2_fmtdesc fmt;

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    printf("========== Supported Formats ==========\n");

    while (xioctl(fd, VIDIOC_ENUM_FMT, &fmt) == 0) {
        printf("[%d] ", fmt.index);
        print_fourcc(fmt.pixelformat);
        printf("  %s\n", fmt.description);

        fmt.index++;
    }

    printf("=======================================\n");
}

static int save_rgb565_raw(const char *path,
                           const unsigned char *data,
                           int width,
                           int height,
                           int bytesperline)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        perror("fopen raw");
        return -1;
    }

    for (int y = 0; y < height; y++) {
        const unsigned char *row = data + y * bytesperline;
        fwrite(row, 1, width * 2, fp);
    }

    fclose(fp);
    return 0;
}

static int save_rgb565_as_ppm(const char *path,
                              const unsigned char *data,
                              int width,
                              int height,
                              int bytesperline)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        perror("fopen ppm");
        return -1;
    }

    fprintf(fp, "P6\n%d %d\n255\n", width, height);

    for (int y = 0; y < height; y++) {
        const unsigned char *row = data + y * bytesperline;

        for (int x = 0; x < width; x++) {
            /*
             * V4L2_PIX_FMT_RGB565 / RGBP:
             * little-endian RGB565
             */
            unsigned short pixel = row[x * 2] | (row[x * 2 + 1] << 8);

            unsigned char r5 = (pixel >> 11) & 0x1F;
            unsigned char g6 = (pixel >> 5) & 0x3F;
            unsigned char b5 = pixel & 0x1F;

            unsigned char r8 = (r5 << 3) | (r5 >> 2);
            unsigned char g8 = (g6 << 2) | (g6 >> 4);
            unsigned char b8 = (b5 << 3) | (b5 >> 2);

            fputc(r8, fp);
            fputc(g8, fp);
            fputc(b8, fp);
        }
    }

    fclose(fp);
    return 0;
}

int main(int argc, char *argv[])
{
    const char *dev_path = "/dev/video1";
    int width = 640;
    int height = 480;
    const char *raw_path = "frame.rgb565";
    const char *ppm_path = "frame.ppm";

    int fd = -1;
    V4L2Buffer buffers[BUFFER_COUNT];

    memset(buffers, 0, sizeof(buffers));

    if (argc >= 2) {
        dev_path = argv[1];
    }

    if (argc >= 4) {
        width = atoi(argv[2]);
        height = atoi(argv[3]);
    }

    if (argc >= 5) {
        raw_path = argv[4];
    }

    if (argc >= 6) {
        ppm_path = argv[5];
    }

    printf("Device : %s\n", dev_path);
    printf("Size   : %dx%d\n", width, height);
    printf("Raw    : %s\n", raw_path);
    printf("PPM    : %s\n", ppm_path);

    fd = open(dev_path, O_RDWR);
    if (fd < 0) {
        perror("open video device");
        return 1;
    }

    enum_formats(fd);

    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));

    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        perror("VIDIOC_QUERYCAP");
        close(fd);
        return 1;
    }

    printf("Driver : %s\n", cap.driver);
    printf("Card   : %s\n", cap.card);
    printf("Bus    : %s\n", cap.bus_info);

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        printf("Error: device does not support video capture\n");
        close(fd);
        return 1;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        printf("Error: device does not support streaming I/O\n");
        close(fd);
        return 1;
    }

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("VIDIOC_S_FMT RGB565");
        printf("Your camera may not support RGB565 at this resolution.\n");
        close(fd);
        return 1;
    }

    printf("Actual format:\n");
    printf("  width       = %d\n", fmt.fmt.pix.width);
    printf("  height      = %d\n", fmt.fmt.pix.height);
    printf("  pixelformat = ");
    print_fourcc(fmt.fmt.pix.pixelformat);
    printf("\n");
    printf("  bytesline   = %d\n", fmt.fmt.pix.bytesperline);
    printf("  sizeimage   = %d\n", fmt.fmt.pix.sizeimage);

    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565) {
        printf("Warning: driver did not return RGB565/RGBP.\n");
        printf("Current format is ");
        print_fourcc(fmt.fmt.pix.pixelformat);
        printf("\n");
        close(fd);
        return 1;
    }

    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));

    req.count = BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("VIDIOC_REQBUFS");
        close(fd);
        return 1;
    }

    if (req.count < 2) {
        printf("Error: insufficient buffer memory\n");
        close(fd);
        return 1;
    }

    for (unsigned int i = 0; i < req.count; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            perror("VIDIOC_QUERYBUF");
            close(fd);
            return 1;
        }

        buffers[i].length = buf.length;
        buffers[i].start = mmap(NULL,
                                buf.length,
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED,
                                fd,
                                buf.m.offset);

        if (buffers[i].start == MAP_FAILED) {
            perror("mmap");
            close(fd);
            return 1;
        }

        if (xioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            perror("VIDIOC_QBUF");
            close(fd);
            return 1;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        perror("VIDIOC_STREAMON");
        close(fd);
        return 1;
    }

    printf("Start streaming, waiting frame...\n");

    fd_set fds;
    struct timeval tv;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    tv.tv_sec = 3;
    tv.tv_usec = 0;

    int ret = select(fd + 1, &fds, NULL, NULL, &tv);
    if (ret < 0) {
        perror("select");
        xioctl(fd, VIDIOC_STREAMOFF, &type);
        close(fd);
        return 1;
    }

    if (ret == 0) {
        printf("Error: select timeout\n");
        xioctl(fd, VIDIOC_STREAMOFF, &type);
        close(fd);
        return 1;
    }

    struct v4l2_buffer frame_buf;
    memset(&frame_buf, 0, sizeof(frame_buf));

    frame_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    frame_buf.memory = V4L2_MEMORY_MMAP;

    if (xioctl(fd, VIDIOC_DQBUF, &frame_buf) < 0) {
        perror("VIDIOC_DQBUF");
        xioctl(fd, VIDIOC_STREAMOFF, &type);
        close(fd);
        return 1;
    }

    printf("Got frame:\n");
    printf("  index     = %d\n", frame_buf.index);
    printf("  bytesused = %d\n", frame_buf.bytesused);

    unsigned char *frame_data = (unsigned char *)buffers[frame_buf.index].start;

    save_rgb565_raw(raw_path,
                    frame_data,
                    fmt.fmt.pix.width,
                    fmt.fmt.pix.height,
                    fmt.fmt.pix.bytesperline);

    save_rgb565_as_ppm(ppm_path,
                       frame_data,
                       fmt.fmt.pix.width,
                       fmt.fmt.pix.height,
                       fmt.fmt.pix.bytesperline);

    printf("Saved raw: %s\n", raw_path);
    printf("Saved ppm: %s\n", ppm_path);

    if (xioctl(fd, VIDIOC_QBUF, &frame_buf) < 0) {
        perror("VIDIOC_QBUF return");
    }

    if (xioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
        perror("VIDIOC_STREAMOFF");
    }

    for (unsigned int i = 0; i < req.count; i++) {
        if (buffers[i].start && buffers[i].start != MAP_FAILED) {
            munmap(buffers[i].start, buffers[i].length);
        }
    }

    close(fd);

    printf("Done.\n");
    return 0;
}
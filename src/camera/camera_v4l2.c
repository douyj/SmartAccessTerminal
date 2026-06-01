/**
 * @file        camera_v4l2.c
 * @brief       Linux V4L2 摄像头驱动（IMX6ULL 真实摄像头）
 * @details     基于 V4L2 框架，使用 RGB565_LE / RGBP 格式采集、
 *              mmap 零拷贝、单帧抓拍保存。
 *
 *              注意：
 *              当前 camera_capture_jpeg() 函数名暂时保留，
 *              但内部实际保存的是 RGB565 裸帧数据。
 *              上层发送协议会通过 image_format="RGB565"
 *              告诉 Mac Qt 按 RGB565 解码显示。
 *
 * @author      嵌入式驱动层
 * @date        2026
 */

#include "camera/camera.h"
#include "common/log.h"

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

/* V4L2 驱动使用的帧缓冲区数量 */
#define V4L2_BUFFER_COUNT 4

/**
 * @brief V4L2 帧缓冲区结构体
 */
typedef struct {
    void *start;
    size_t length;
} V4L2Buffer;


/* 全局驱动状态 */
static int g_fd = -1;
static V4L2Buffer g_buffers[V4L2_BUFFER_COUNT];
static int g_buffer_count = 0;
static int g_camera_inited = 0;


/**
 * @brief 打印 fourcc 方便调试
 */
static void fourcc_to_str(unsigned int pixelformat, char out[5])
{
    out[0] = pixelformat & 0xFF;
    out[1] = (pixelformat >> 8) & 0xFF;
    out[2] = (pixelformat >> 16) & 0xFF;
    out[3] = (pixelformat >> 24) & 0xFF;
    out[4] = '\0';
}


/**
 * @brief  封装 ioctl，自动处理 EINTR 中断
 */
static int xioctl(int fd, unsigned long request, void *arg)
{
    int ret;

    do {
        ret = ioctl(fd, request, arg);
    } while (ret < 0 && errno == EINTR);

    return ret;
}


/**
 * @brief  等待一帧数据就绪
 */
static int wait_frame_ready(int fd)
{
    fd_set fds;
    struct timeval tv;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    tv.tv_sec = 2;
    tv.tv_usec = 0;

    int ret = select(fd + 1, &fds, NULL, NULL, &tv);
    if (ret < 0) {
        LOG_ERROR("[V4L2] select failed: %s", strerror(errno));
        return -1;
    }

    if (ret == 0) {
        LOG_ERROR("[V4L2] select timeout");
        return -1;
    }

    return 0;
}


/**
 * @brief  将 RGB565 裸帧保存到文件
 * @param  path  保存路径
 * @param  data  RGB565 数据指针
 * @param  size  数据长度
 * @return 0成功，-1失败
 */
static int save_rgb565_file(const char *path, const void *data, size_t size)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        LOG_ERROR("[V4L2] fopen failed: %s, err=%s", path, strerror(errno));
        return -1;
    }

    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);

    if (written != size) {
        LOG_ERROR("[V4L2] fwrite failed, written=%zu, expected=%zu", written, size);
        return -1;
    }

    return 0;
}


/**
 * @brief  设置摄像头格式：RGB565 + 自定义分辨率
 */
static int camera_set_format(int fd, int width, int height)
{
    struct v4l2_format fmt;
    char fourcc[5];

    memset(&fmt, 0, sizeof(fmt));

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;

    /*
     * RGB565_LE，在 V4L2 里通常显示为 RGBP。
     * 你的开发板日志里 pixfmt=0x50424752，也就是 RGBP。
     */
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        LOG_ERROR("[V4L2] VIDIOC_S_FMT RGB565 failed: %s", strerror(errno));
        return -1;
    }

    fourcc_to_str(fmt.fmt.pix.pixelformat, fourcc);

    LOG_INFO("[V4L2] format set: %dx%d, pixfmt=0x%08x(%s)",
             fmt.fmt.pix.width,
             fmt.fmt.pix.height,
             fmt.fmt.pix.pixelformat,
             fourcc);

    LOG_INFO("[V4L2] bytesperline=%u, sizeimage=%u",
             fmt.fmt.pix.bytesperline,
             fmt.fmt.pix.sizeimage);

    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565) {
        LOG_ERROR("[V4L2] camera did not accept RGB565 format, actual=0x%08x(%s)",
                  fmt.fmt.pix.pixelformat,
                  fourcc);
        return -1;
    }

    return 0;
}


/**
 * @brief  初始化 mmap 映射
 */
static int camera_init_mmap(int fd)
{
    struct v4l2_requestbuffers req;

    memset(&req, 0, sizeof(req));

    req.count = V4L2_BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        LOG_ERROR("[V4L2] VIDIOC_REQBUFS failed: %s", strerror(errno));
        return -1;
    }

    if (req.count < 2) {
        LOG_ERROR("[V4L2] insufficient buffer memory");
        return -1;
    }

    g_buffer_count = req.count;

    for (int i = 0; i < g_buffer_count; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            LOG_ERROR("[V4L2] VIDIOC_QUERYBUF failed: %s", strerror(errno));
            return -1;
        }

        g_buffers[i].length = buf.length;
        g_buffers[i].start = mmap(NULL,
                                  buf.length,
                                  PROT_READ | PROT_WRITE,
                                  MAP_SHARED,
                                  fd,
                                  buf.m.offset);

        if (g_buffers[i].start == MAP_FAILED) {
            LOG_ERROR("[V4L2] mmap failed: %s", strerror(errno));
            g_buffers[i].start = NULL;
            return -1;
        }

        LOG_INFO("[V4L2] mmap buffer %d, length=%zu", i, g_buffers[i].length);
    }

    return 0;
}


/**
 * @brief  将所有缓冲区入队
 */
static int camera_queue_buffers(int fd)
{
    for (int i = 0; i < g_buffer_count; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            LOG_ERROR("[V4L2] VIDIOC_QBUF failed: %s", strerror(errno));
            return -1;
        }
    }

    return 0;
}


/**
 * @brief  开启摄像头数据流
 */
static int camera_stream_on(int fd)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        LOG_ERROR("[V4L2] VIDIOC_STREAMON failed: %s", strerror(errno));
        return -1;
    }

    LOG_INFO("[V4L2] stream on");
    return 0;
}


/**
 * @brief  关闭摄像头数据流
 */
static int camera_stream_off(int fd)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (fd < 0) {
        return 0;
    }

    if (xioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
        LOG_WARN("[V4L2] VIDIOC_STREAMOFF failed: %s", strerror(errno));
        return -1;
    }

    LOG_INFO("[V4L2] stream off");
    return 0;
}


/**
 * @brief  V4L2 摄像头初始化
 */
int camera_init(const char *dev_name, int width, int height)
{
    if (g_camera_inited) {
        LOG_WARN("[V4L2] camera already initialized");
        return 0;
    }

    if (dev_name == NULL || dev_name[0] == '\0') {
        LOG_ERROR("[V4L2] invalid device name");
        return -1;
    }

    memset(g_buffers, 0, sizeof(g_buffers));
    g_buffer_count = 0;

    /* 1. 打开设备 */
    g_fd = open(dev_name, O_RDWR);
    if (g_fd < 0) {
        LOG_ERROR("[V4L2] open %s failed: %s", dev_name, strerror(errno));
        return -1;
    }

    LOG_INFO("[V4L2] open device: %s", dev_name);

    /* 2. 查询设备能力 */
    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));

    if (xioctl(g_fd, VIDIOC_QUERYCAP, &cap) < 0) {
        LOG_ERROR("[V4L2] VIDIOC_QUERYCAP failed: %s", strerror(errno));
        close(g_fd);
        g_fd = -1;
        return -1;
    }

    LOG_INFO("[V4L2] driver=%s, card=%s, bus=%s",
             cap.driver,
             cap.card,
             cap.bus_info);

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        LOG_ERROR("[V4L2] device does not support video capture");
        close(g_fd);
        g_fd = -1;
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        LOG_ERROR("[V4L2] device does not support streaming I/O");
        close(g_fd);
        g_fd = -1;
        return -1;
    }

    /* 3. 设置 RGB565 图像格式 */
    if (camera_set_format(g_fd, width, height) < 0) {
        close(g_fd);
        g_fd = -1;
        return -1;
    }

    /* 4. 初始化 mmap 缓冲区 */
    if (camera_init_mmap(g_fd) < 0) {
        camera_deinit();
        return -1;
    }

    /* 5. 缓冲区入队 */
    if (camera_queue_buffers(g_fd) < 0) {
        camera_deinit();
        return -1;
    }

    /* 6. 开启流 */
    if (camera_stream_on(g_fd) < 0) {
        camera_deinit();
        return -1;
    }

    g_camera_inited = 1;

    LOG_INFO("[V4L2] camera init success");
    return 0;
}


/**
 * @brief  抓拍一帧 RGB565 并保存为文件
 *
 * @note   函数名暂时保留为 camera_capture_jpeg，
 *         是为了兼容 app_main.c 和 camera.h 的旧接口。
 *         但当前实际保存的是 RGB565 裸数据。
 */
int camera_capture_jpeg(const char *save_path)
{
    if (!g_camera_inited || g_fd < 0) {
        LOG_ERROR("[V4L2] camera not initialized");
        return -1;
    }

    if (save_path == NULL || save_path[0] == '\0') {
        LOG_ERROR("[V4L2] invalid save path");
        return -1;
    }

    /* 等待帧就绪 */
    if (wait_frame_ready(g_fd) < 0) {
        return -1;
    }

    /* 出队，获取一帧数据 */
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (xioctl(g_fd, VIDIOC_DQBUF, &buf) < 0) {
        LOG_ERROR("[V4L2] VIDIOC_DQBUF failed: %s", strerror(errno));
        return -1;
    }

    LOG_INFO("[V4L2] frame captured: index=%d, bytesused=%u",
             buf.index,
             buf.bytesused);

    if (buf.index >= (unsigned int)g_buffer_count || buf.bytesused <= 0) {
        LOG_ERROR("[V4L2] invalid frame");
        xioctl(g_fd, VIDIOC_QBUF, &buf);
        return -1;
    }

    /*
     * RGB565 裸帧保存。
     * 对 640x480 RGB565，大小一般是 640 * 480 * 2 = 614400。
     */
    int ret = save_rgb565_file(save_path,
                               g_buffers[buf.index].start,
                               buf.bytesused);

    /* 重新入队 */
    if (xioctl(g_fd, VIDIOC_QBUF, &buf) < 0) {
        LOG_WARN("[V4L2] VIDIOC_QBUF return failed: %s", strerror(errno));
    }

    if (ret < 0) {
        LOG_ERROR("[V4L2] save rgb565 failed");
        return -1;
    }

    LOG_INFO("[V4L2] rgb565 saved: %s", save_path);
    return 0;
}


/**
 * @brief  反初始化，释放所有资源
 */
int camera_deinit(void)
{
    if (g_fd >= 0) {
        camera_stream_off(g_fd);
    }

    for (int i = 0; i < g_buffer_count; i++) {
        if (g_buffers[i].start != NULL) {
            munmap(g_buffers[i].start, g_buffers[i].length);
            g_buffers[i].start = NULL;
            g_buffers[i].length = 0;
        }
    }

    g_buffer_count = 0;

    if (g_fd >= 0) {
        close(g_fd);
        g_fd = -1;
    }

    g_camera_inited = 0;

    LOG_INFO("[V4L2] camera deinit");
    return 0;
}

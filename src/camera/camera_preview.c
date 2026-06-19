#include "camera/camera_preview.h"
#include "common/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>

#include <linux/videodev2.h>

#define PREVIEW_BUFFER_COUNT 4

typedef struct{
    void *start;
    size_t length;
}PreviewBuffer;

static int g_fd = -1;
static PreviewBuffer g_buffers[PREVIEW_BUFFER_COUNT];
static int g_buffer_count = 0;

static pthread_t g_preview_thread;  //采集线程句柄
static int g_preview_running = 0;   //线程运行标记
static int g_thread_created = 0;    //线程创建标记

static pthread_mutex_t g_frame_mutex = PTHREAD_MUTEX_INITIALIZER;   //互斥锁
static unsigned char *g_latest_frame = NULL;        //缓存最新一帧图像
static int g_latest_frame_size = 0;
static int g_frame_width = 0;
static int g_frame_height = 0;
static int g_has_frame = 0;     //是否存在有效帧

static int g_fps = 0;       //当前帧数

// 封装ioctl，信号中断自动重试
static int xioctl(int fd, unsigned long request, void *arg)
{
    int ret;

    do {
        ret = ioctl(fd, request, arg);
    } while (ret < 0 && errno == EINTR);

    return ret;
}


// fourcc编码转字符串，日志打印用
static void fourcc_to_str(unsigned int pixelformat, char out[5])
{
    out[0] = pixelformat & 0xFF;
    out[1] = (pixelformat >> 8) & 0xFF;
    out[2] = (pixelformat >> 16) & 0xFF;
    out[3] = (pixelformat >> 24) & 0xFF;
    out[4] = '\0';
}


// select阻塞等待帧就绪，2秒超时
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
        LOG_ERROR("[PREVIEW] select failed: %s", strerror(errno));
        return -1;
    }

    if (ret == 0) {
        LOG_ERROR("[PREVIEW] select timeout");
        return -1;
    }

    return 0;
}


// 设置采集格式：RGB565+指定分辨率
static int preview_set_format(int fd, int width, int height)
{
    struct v4l2_format fmt;
    char fourcc[5];

    memset(&fmt, 0, sizeof(fmt));

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        LOG_ERROR("[PREVIEW] VIDIOC_S_FMT RGB565 failed: %s", strerror(errno));
        return -1;
    }

    fourcc_to_str(fmt.fmt.pix.pixelformat, fourcc);

    LOG_INFO("[PREVIEW] format set: %dx%d, pixfmt=0x%08x(%s)",
             fmt.fmt.pix.width,
             fmt.fmt.pix.height,
             fmt.fmt.pix.pixelformat,
             fourcc);

    LOG_INFO("[PREVIEW] bytesperline=%u, sizeimage=%u",
             fmt.fmt.pix.bytesperline,
             fmt.fmt.pix.sizeimage);

    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565) {
        LOG_ERROR("[PREVIEW] camera did not accept RGB565, actual=0x%08x(%s)",
                  fmt.fmt.pix.pixelformat,
                  fourcc);
        return -1;
    }

    g_frame_width = fmt.fmt.pix.width;
    g_frame_height = fmt.fmt.pix.height;
    g_latest_frame_size = fmt.fmt.pix.sizeimage;

    if (g_latest_frame_size <= 0) {
        g_latest_frame_size = g_frame_width * g_frame_height * 2;
    }

    return 0;
}

// 申请内核缓冲并mmap映射到用户空间
static int preview_init_mmap(int fd)
{
    struct v4l2_requestbuffers req;

    memset(&req, 0, sizeof(req));

    req.count = PREVIEW_BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        LOG_ERROR("[PREVIEW] VIDIOC_REQBUFS failed: %s", strerror(errno));
        return -1;
    }

    if (req.count < 2) {
        LOG_ERROR("[PREVIEW] insufficient buffer memory");
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
            LOG_ERROR("[PREVIEW] VIDIOC_QUERYBUF failed: %s", strerror(errno));
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
            LOG_ERROR("[PREVIEW] mmap failed: %s", strerror(errno));
            g_buffers[i].start = NULL;
            return -1;
        }

        LOG_INFO("[PREVIEW] mmap buffer %d, length=%zu",
                 i,
                 g_buffers[i].length);
    }

    return 0;
}


// 将所有缓冲入队交给内核
static int preview_queue_buffers(int fd)
{
    for (int i = 0; i < g_buffer_count; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            LOG_ERROR("[PREVIEW] VIDIOC_QBUF failed: %s", strerror(errno));
            return -1;
        }
    }

    return 0;
}

// 开启摄像头数据流
static int preview_stream_on(int fd)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        LOG_ERROR("[PREVIEW] VIDIOC_STREAMON failed: %s", strerror(errno));
        return -1;
    }

    LOG_INFO("[PREVIEW] stream on");
    return 0;
}

// 关闭摄像头数据流
static void preview_stream_off(int fd)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (fd < 0) {
        return;
    }

    if (xioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
        LOG_WARN("[PREVIEW] VIDIOC_STREAMOFF failed: %s", strerror(errno));
    } else {
        LOG_INFO("[PREVIEW] stream off");
    }
}


// 统一释放所有资源
static void preview_cleanup(void)
{
    preview_stream_off(g_fd);

    for (int i = 0; i < g_buffer_count; i++) {
        if (g_buffers[i].start) {
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

    pthread_mutex_lock(&g_frame_mutex);

    if (g_latest_frame) {
        free(g_latest_frame);
        g_latest_frame = NULL;
    }

    g_latest_frame_size = 0;
    g_frame_width = 0;
    g_frame_height = 0;
    g_has_frame = 0;

    pthread_mutex_unlock(&g_frame_mutex);
}


// 采集线程循环函数
static void *preview_thread_func(void *arg)
{
    (void)arg;

    int frame_count = 0;
    time_t last_time = time(NULL);

    LOG_INFO("[PREVIEW] thread started");

    while (g_preview_running) {
        if (wait_frame_ready(g_fd) < 0) {
            usleep(10 * 1000);
            continue;
        }

        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (xioctl(g_fd, VIDIOC_DQBUF, &buf) < 0) {
            LOG_ERROR("[PREVIEW] VIDIOC_DQBUF failed: %s", strerror(errno));
            continue;
        }

        if (buf.index < (unsigned int)g_buffer_count &&
            buf.bytesused > 0 &&
            g_buffers[buf.index].start != NULL) {

            int copy_size = buf.bytesused;

            if (copy_size > g_latest_frame_size) {
                copy_size = g_latest_frame_size;
            }

            pthread_mutex_lock(&g_frame_mutex);

            if (g_latest_frame && copy_size > 0) {
                memcpy(g_latest_frame, g_buffers[buf.index].start, copy_size);
                g_has_frame = 1;
            }

            pthread_mutex_unlock(&g_frame_mutex);

            frame_count++;

            time_t now = time(NULL);
            if (now != last_time) {
                g_fps = frame_count;
                frame_count = 0;
                last_time = now;
            }
        }

        if (xioctl(g_fd, VIDIOC_QBUF, &buf) < 0) {
            LOG_WARN("[PREVIEW] VIDIOC_QBUF return failed: %s", strerror(errno));
        }
    }

    LOG_INFO("[PREVIEW] thread stopped");
    return NULL;
}


// 启动采集入口
int camera_preview_start(const char *dev_name, int width, int height)
{
    if (g_preview_running) {
        LOG_WARN("[PREVIEW] already running");
        return 0;
    }

    if (!dev_name || dev_name[0] == '\0') {
        LOG_ERROR("[PREVIEW] invalid device name");
        return -1;
    }

    memset(g_buffers, 0, sizeof(g_buffers));
    g_buffer_count = 0;
    g_fps = 0;

    g_fd = open(dev_name, O_RDWR);
    if (g_fd < 0) {
        LOG_ERROR("[PREVIEW] open %s failed: %s", dev_name, strerror(errno));
        return -1;
    }

    LOG_INFO("[PREVIEW] open device: %s", dev_name);

    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));

    if (xioctl(g_fd, VIDIOC_QUERYCAP, &cap) < 0) {
        LOG_ERROR("[PREVIEW] VIDIOC_QUERYCAP failed: %s", strerror(errno));
        preview_cleanup();
        return -1;
    }

    LOG_INFO("[PREVIEW] driver=%s, card=%s, bus=%s",
             cap.driver,
             cap.card,
             cap.bus_info);

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        LOG_ERROR("[PREVIEW] device does not support video capture");
        preview_cleanup();
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        LOG_ERROR("[PREVIEW] device does not support streaming I/O");
        preview_cleanup();
        return -1;
    }

    if (preview_set_format(g_fd, width, height) < 0) {
        preview_cleanup();
        return -1;
    }

    pthread_mutex_lock(&g_frame_mutex);

    g_latest_frame = (unsigned char *)malloc(g_latest_frame_size);
    if (!g_latest_frame) {
        pthread_mutex_unlock(&g_frame_mutex);
        LOG_ERROR("[PREVIEW] malloc latest frame failed");
        preview_cleanup();
        return -1;
    }

    memset(g_latest_frame, 0, g_latest_frame_size);
    g_has_frame = 0;

    pthread_mutex_unlock(&g_frame_mutex);

    if (preview_init_mmap(g_fd) < 0) {
        preview_cleanup();
        return -1;
    }

    if (preview_queue_buffers(g_fd) < 0) {
        preview_cleanup();
        return -1;
    }

    if (preview_stream_on(g_fd) < 0) {
        preview_cleanup();
        return -1;
    }

    g_preview_running = 1;

    if (pthread_create(&g_preview_thread, NULL, preview_thread_func, NULL) != 0) {
        LOG_ERROR("[PREVIEW] pthread_create failed");
        g_preview_running = 0;
        preview_cleanup();
        return -1;
    }

    g_thread_created = 1;

    LOG_INFO("[PREVIEW] start success");
    return 0;
}

// 停止采集入口
void camera_preview_stop(void)
{
    if (!g_preview_running && !g_thread_created) {
        return;
    }

    g_preview_running = 0;

    if (g_thread_created) {
        pthread_join(g_preview_thread, NULL);
        g_thread_created = 0;
    }

    preview_cleanup();

    LOG_INFO("[PREVIEW] stop done");
}

// 获取最新一帧图像数据
int camera_preview_get_frame(unsigned char *out_buf,
                             int out_size,
                             int *width,
                             int *height,
                             int *frame_size)
{
    if (!out_buf || out_size <= 0) {
        return -1;
    }

    pthread_mutex_lock(&g_frame_mutex);

    if (!g_latest_frame || !g_has_frame || g_latest_frame_size <= 0) {
        pthread_mutex_unlock(&g_frame_mutex);
        return -1;
    }

    if (out_size < g_latest_frame_size) {
        pthread_mutex_unlock(&g_frame_mutex);
        return -1;
    }

    memcpy(out_buf, g_latest_frame, g_latest_frame_size);

    if (width) {
        *width = g_frame_width;
    }

    if (height) {
        *height = g_frame_height;
    }

    if (frame_size) {
        *frame_size = g_latest_frame_size;
    }

    pthread_mutex_unlock(&g_frame_mutex);
    return 0;
}

// 查询分辨率、帧大小信息
int camera_preview_get_info(int *width, int *height, int *frame_size)
{
    pthread_mutex_lock(&g_frame_mutex);

    if (width) {
        *width = g_frame_width;
    }

    if (height) {
        *height = g_frame_height;
    }

    if (frame_size) {
        *frame_size = g_latest_frame_size;
    }

    pthread_mutex_unlock(&g_frame_mutex);

    return 0;
}

// 判断采集是否正在运行
int camera_preview_is_running(void)
{
    return g_preview_running;
}

// 获取当前实时帧率
int camera_preview_get_fps(void)
{
    return g_fps;
}


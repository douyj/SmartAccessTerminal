#include "network/packet.h"
#include "network/tcp_client.h"
#include "common/log.h"
#include "third_party/cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>

static int read_file_to_buffer(const char *path, unsigned char **out_buf, int *out_size)
{
    FILE *fp = NULL;
    long size = 0;
    unsigned char *buf = NULL;

    if (!path || !out_buf || !out_size) {
        LOG_ERROR("read_file_to_buffer invalid args");
        return -1;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        LOG_ERROR("open image failed: %s", path);
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        LOG_ERROR("fseek end failed: %s", path);
        return -1;
    }

    size = ftell(fp);
    if (size <= 0) {
        fclose(fp);
        LOG_ERROR("invalid image file size: %s", path);
        return -1;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        LOG_ERROR("fseek set failed: %s", path);
        return -1;
    }

    buf = (unsigned char *)malloc((size_t)size);
    if (!buf) {
        fclose(fp);
        LOG_ERROR("malloc image buffer failed, size=%ld", size);
        return -1;
    }

    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        fclose(fp);
        free(buf);
        LOG_ERROR("read image failed: %s", path);
        return -1;
    }

    fclose(fp);

    *out_buf = buf;
    *out_size = (int)size;

    return 0;
}

static const char *get_filename_from_path(const char *path)
{
    const char *p = NULL;

    if (!path) {
        return "unknown";
    }

    p = strrchr(path, '/');
    if (p) {
        return p + 1;
    }

    return path;
}

int packet_send_image_ex(int sockfd,
                         const char *image_path,
                         const char *device_id,
                         const char *image_format,
                         int width,
                         int height)
{
    unsigned char *image_buf = NULL;
    int image_size = 0;

    cJSON *root = NULL;
    char *json_str = NULL;

    int json_len = 0;
    uint32_t json_len_net = 0;

    int ret = -1;

    if (!image_path || !device_id || !image_format) {
        LOG_ERROR("packet_send_image_ex invalid args");
        return -1;
    }

    if (read_file_to_buffer(image_path, &image_buf, &image_size) < 0) {
        return -1;
    }

    LOG_INFO("send image: %s", image_path);
    LOG_INFO("image format: %s", image_format);
    LOG_INFO("image width : %d", width);
    LOG_INFO("image height: %d", height);
    LOG_INFO("image size : %d bytes", image_size);

    root = cJSON_CreateObject();
    if (!root) {
        LOG_ERROR("cJSON_CreateObject failed");
        goto cleanup;
    }

    cJSON_AddStringToObject(root, "type", "snapshot");
    cJSON_AddStringToObject(root, "device_id", device_id);
    cJSON_AddStringToObject(root, "filename", get_filename_from_path(image_path));
    cJSON_AddStringToObject(root, "image_format", image_format);
    cJSON_AddNumberToObject(root, "width", width);
    cJSON_AddNumberToObject(root, "height", height);
    cJSON_AddNumberToObject(root, "image_size", image_size);

    json_str = cJSON_PrintUnformatted(root);
    if (!json_str) {
        LOG_ERROR("cJSON_PrintUnformatted failed");
        goto cleanup;
    }

    json_len = (int)strlen(json_str);
    json_len_net = htonl((uint32_t)json_len);

    LOG_INFO("json header length: %d", json_len);
    LOG_INFO("json header: %s", json_str);

    /*
     * 协议：
     * [4字节 JSON 长度][JSON 头][图像数据]
     */
    if (tcp_send_all(sockfd, &json_len_net, 4) < 0) {
        LOG_ERROR("send json length failed");
        goto cleanup;
    }

    if (tcp_send_all(sockfd, json_str, json_len) < 0) {
        LOG_ERROR("send json header failed");
        goto cleanup;
    }

    if (tcp_send_all(sockfd, image_buf, image_size) < 0) {
        LOG_ERROR("send image data failed");
        goto cleanup;
    }

    LOG_INFO("image send success");
    ret = 0;

cleanup:
    if (json_str) {
        free(json_str);
    }

    if (root) {
        cJSON_Delete(root);
    }

    if (image_buf) {
        free(image_buf);
    }

    return ret;
}

int packet_send_image(int sockfd,
                      const char *image_path,
                      const char *device_id)
{
    /*
     * 旧测试图 dyj.jpg / unknown.jpg 仍然按 JPEG 发送。
     */
    return packet_send_image_ex(sockfd,
                                image_path,
                                device_id,
                                "JPEG",
                                0,
                                0);
}

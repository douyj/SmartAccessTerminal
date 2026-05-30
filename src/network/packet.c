#include "network/packet.h"
#include "network/tcp_client.h"
#include "protocol/access_protocol.h"
#include "common/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>

/*获取路径中的文件名*/
static const char *get_filename_from_path(const char *path)
{
    /*找到最后一个'/'*/
    const char *p = strrchr(path, '/');
    if(p == NULL)
    {
        return path;
    }

    return p+1;
}

/*获取文件大小*/
static int get_file_size(FILE *fp)
{
    /*把文件指针 移动到 文件的最后面*/
    if(fseek(fp, 0, SEEK_END) != 0)
    {
        return -1;
    }

    long size = ftell(fp);
    if(size < 0){
        return -1;
    }

    rewind(fp);
    return (int)size;
}

/**
 * @brief      通过TCP发送本地JPG图片至服务器
 * @param      sockfd       已建立连接的TCP套接字
 * @param      jpg_path     本地图片完整路径
 * @param      device_id    设备唯一标识ID，用于服务器区分设备
 * @return     成功返回0，失败返回-1
 * @details    发送协议格式：
 *             1. 4字节网络序JSON头长度
 *             2. JSON头部(包含设备ID、文件名、图片大小)
 *             3. JPG图片二进制原始数据
 * @note       依赖tcp_send_all实现可靠发送，内部自动申请/释放图片内存
 */
int packet_send_image(int sockfd, const char *jpg_path, const char *device_id)
{
    FILE *fp = fopen(jpg_path, "rb");
    if(fp == NULL)
    {
        LOG_ERROR("open image failed: %s", jpg_path);
        return -1;
    }

    int image_size = get_file_size(fp);
    if(image_size <= 0){
         LOG_ERROR("invalid image size:%d", image_size);
         fclose(fp);
         return -1;
    }

    char *image_buf = (char *)malloc(image_size);
    if(image_buf == NULL)
    {
        LOG_ERROR("malloc image buffer failed");
        fclose(fp);
        return -1;
    }

    int read_size = fread(image_buf, 1, image_size, fp);
    fclose(fp);

    if(read_size != image_size){
        LOG_ERROR("read image failed, read=%%d, expected=%d", read_size, image_size);
        free(image_buf);
        return -1;
    }

    char json_header[512];
    const char *filename = get_filename_from_path(jpg_path);

    //给服务器看的内容
    int json_len = access_build_snapshot_header(
        json_header,
        sizeof(json_header),
        device_id,
        filename,
        image_size
    );

    if (json_len <= 0 || json_len >= (int)sizeof(json_header)) {
        LOG_ERROR("build json header failed");
        free(image_buf);
        return -1;
    }

    uint32_t net_json_len = htonl((uint32_t)json_len);

    LOG_INFO("send image: %s", jpg_path);
    LOG_INFO("image size: %d bytes", image_size);
    LOG_INFO("json header length: %d", json_len);
    LOG_DEBUG("json header: %s", json_header);

    if (tcp_send_all(sockfd, &net_json_len, sizeof(net_json_len)) != sizeof(net_json_len)) {
        LOG_ERROR("send json length failed");
        free(image_buf);
        return -1;
    }

    //发送数据
    if (tcp_send_all(sockfd, json_header, json_len) != json_len) {
        LOG_ERROR("send json header failed");
        free(image_buf);
        return -1;
    }

    //发送图片
    if (tcp_send_all(sockfd, image_buf, image_size) != image_size) {
        LOG_ERROR("send image data failed");
        free(image_buf);
        return -1;
    }

    free(image_buf);

    LOG_INFO("image send success");
    return 0;
}
#ifndef PACKET_H
#define PACKET_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 兼容旧接口：
 * 默认按 JPEG 发送。
 */
int packet_send_image(int sockfd,
                      const char *image_path,
                      const char *device_id);

/*
 * 新接口：
 * 支持 JPEG / RGB565 等格式。
 *
 * 协议格式：
 * [4字节 JSON 长度，大端]
 * [JSON 头]
 * [图像二进制数据]
 */
int packet_send_image_ex(int sockfd,
                         const char *image_path,
                         const char *device_id,
                         const char *image_format,
                         int width,
                         int height);

#ifdef __cplusplus
}
#endif

#endif

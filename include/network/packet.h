#ifndef PACKET_H
#define PACKET_H

int packet_send_image(int sockfd, const char *jpg_path, const char *device_id);

#endif
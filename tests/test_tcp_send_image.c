#include "network/tcp_client.h"
#include "network/packet.h"
#include "common/log.h"

#include "bsp/door.h"
#include "bsp/alarm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static void handle_access_result(const char *json_result)
{
    LOG_INFO("server result raw: %s", json_result);

    if (strstr(json_result, "\"result\":\"allow\"") != NULL ||
        strstr(json_result, "\"result\": \"allow\"") != NULL) {

        LOG_INFO("识别结果: 允许通行");
        LOG_INFO("执行动作: door_open(3000)");

        door_open(3000);

    } else if (strstr(json_result, "\"result\":\"deny\"") != NULL ||
               strstr(json_result, "\"result\": \"deny\"") != NULL) {

        LOG_INFO("识别结果: 拒绝通行");
        LOG_INFO("执行动作: alarm_beep(1000)");

        alarm_beep(1000);

    } else if (strstr(json_result, "\"result\":\"no_face\"") != NULL ||
               strstr(json_result, "\"result\": \"no_face\"") != NULL) {

        LOG_INFO("识别结果: 未检测到人脸");
        LOG_INFO("执行动作: none");

    } else {
        LOG_ERROR("未知识别结果: %s", json_result);
    }
}

int main(int argc, char *argv[])
{
    log_set_level(LOG_LEVEL_INFO);
    log_set_show_detail(0);

    if (argc != 4) {
        printf("Usage:\n");
        printf("  %s <server_ip> <server_port> <jpg_path>\n", argv[0]);
        printf("\nExample:\n");
        printf("  %s 127.0.0.1 9000 ./test_data/dyj.jpg\n", argv[0]);
        return -1;
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    const char *jpg_path = argv[3];

    const char *device_id = "imx6ull_001";

    door_init();
    alarm_init();

    LOG_INFO("server ip   : %s", server_ip);
    LOG_INFO("server port : %d", server_port);
    LOG_INFO("jpg path    : %s", jpg_path);

    int sockfd = tcp_client_connect(server_ip, server_port);
    if (sockfd < 0) {
        LOG_ERROR("connect server failed");
        return -1;
    }

    LOG_INFO("connect server success");

    if (packet_send_image(sockfd, jpg_path, device_id) < 0) {
        LOG_ERROR("send image failed");
        tcp_client_close(sockfd);
        return -1;
    }

    LOG_INFO("send image success, waiting for result...");

    char result_buf[1024];

    int n = tcp_recv_line(sockfd, result_buf, sizeof(result_buf));
    if (n <= 0) {
        LOG_ERROR("recv result failed");
        tcp_client_close(sockfd);
        return -1;
    }

    handle_access_result(result_buf);

    tcp_client_close(sockfd);

    door_deinit();
    alarm_deinit();

    LOG_INFO("client exit");
    return 0;
}
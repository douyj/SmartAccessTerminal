#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

int tcp_client_connect(const char *server_ip, int server_port);
int tcp_send_all(int sockfd, const void *buf, int len);
int tcp_recv_all(int sockfd, void *buf, int len);
int tcp_recv_line(int sockfd, char *buf, int max_len);
void tcp_client_close(int sockfd);

#endif
#include "network/tcp_client.h"
#include "common/log.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>


/**
 * @brief  创建TCP客户端套接字并连接到指定服务器
 * @param  server_ip   服务器IP地址(字符串格式,如"192.168.1.100")
 * @param  server_port 服务器端口号
 * @return 成功返回已连接的TCP套接字文件描述符(>0),失败返回-1
 * @note   调用者需要在使用完毕后调用close()关闭返回的套接字
 */
int tcp_client_connect(const char *server_ip, int server_port)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        LOG_ERROR("socket failed: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    /*将字符串ip转成二进制格式，返回1表示成功*/
    if(inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
    {
        LOG_ERROR("invalid server ip: %s", server_ip);
        close(sockfd);
        return -1;
    }

    /*连接服务器*/
    if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        LOG_ERROR("connect failed: %s", strerror(errno));
        close(sockfd);
        return -1;
    }

    LOG_INFO("connected to server: %s:%d", server_ip, server_port);
    return sockfd;

}


/**
 * @brief  保证TCP发送指定长度的所有数据（解决send不完整问题）
 * @param  sockfd  已连接的TCP套接字
 * @param  buf     发送数据缓冲区
 * @param  len     要发送的字节数
 * @return 成功返回发送的总字节数(等于len)，失败返回-1
 * @note   适用于图片、结构体、文件等任意二进制数据发送
 */
int tcp_send_all(int sockfd, const void *buf, int len)
{
    int total = 0;
    const char *p = (const char *)buf;      //数据起始位置

    while(total < len)
    {
        int n = send(sockfd, p + total, len - total, 0);
        if(n < 0)
        {
            if(errno == EINTR) continue;
            LOG_ERROR("send failed: %s", strerror(errno));
            return -1;
        }

        if(n == 0)
        {
            LOG_ERROR("send returned 0");
            return -1;
        }

        total += n;
    }

    return total;
}

/**
 * @brief  保证TCP接收指定长度的数据（解决recv接收不完整问题）
 * @param  sockfd 已连接的TCP套接字
 * @param  buf    接收数据缓冲区
 * @param  len    想要接收的字节数
 * @return 成功返回总接收字节数(等于len)，失败返回-1
 * @note   配合tcp_send_all使用，专门接收图片、结构体、帧数据
 */
int tcp_recv_all(int sockfd, void *buf, int len)
{
    int total = 0;
    char *p = (char *)buf; 

    while(total < len)
    {
        int n = recv(sockfd, p+total, len - total, 0);
        if(n<0)
        {
            if(errno == EINTR) continue;
            LOG_ERROR("recv failed: %s", strerror(errno));
            return -1;
        }

        if(n == 0)
        {
            LOG_ERROR("server closed connection");
            return -1;
        }

        total += n;
    }

    return total;
}

/**
 * @brief  TCP按行接收，直到遇到换行符\n
 * @param  sockfd   已连接TCP套接字
 * @param  buf      存储接收字符串的缓冲区
 * @param  max_len  缓冲区最大长度
 * @return 成功返回接收到的字符数，断开连接返回0，出错返回-1
 * @note   自动在末尾添加\0，适合读取文本行、AT指令、HTTP头部
 */
int tcp_recv_line(int sockfd, char *buf, int max_len)
{
    int idx = 0;

    while(idx < max_len - 1)
    {
        char ch;
        int n = recv(sockfd, &ch, 1, 0);    //每次只读1个字节

        if(n<0)
        {
            if(errno == EINTR) continue;
            LOG_ERROR("recv line failed: %s", strerror(errno));
            return -1;
        }

        if(n == 0) break;

        if(ch == '\n') break;

        buf[idx++] = ch;
        
    }

    buf[idx] = '\0';
    return idx;
}

void tcp_client_close(int sockfd)
{
    if(sockfd >= 0)
    {
        close(sockfd);
    }
}
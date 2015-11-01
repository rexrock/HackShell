/*
 * * author：liyang
 * * encode: utf-8
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include "nl_hack.h"
#include "hack_client.h"

/*
 * 功  能：向服务端发送本地产生的劫持日志
 * 参  数：sock_fd, 与服务端建立连接的sockst
 *         msg, 要发送的日志信息
 * 返回值： 0，发送成功
 *         -1，发送失败
 */
int handle_send(int sock_fd, const char *msg)
{
    int ret, save_errno, send_index, send_len;
    if (0 < sock_fd && msg && '\0' != *msg)
    {
        send_len = strlen(msg);
        char head_buff[32];
        snprintf(head_buff, 32, "hack:");
        send_index = 1;
        ret = send(sock_fd, head_buff, 8, 0);
        HACK_DEBUG(0, "[%d:%d] send_buff is \"%s\"\n", send_index, ret, head_buff);
        if (8 == ret)
        {
            snprintf(head_buff, 32, "%08d", send_len);
            send_index = 2;
            ret = send(sock_fd, head_buff, 10, 0);
            HACK_DEBUG(0, "[%d:%d] send_buff is \"%s\"\n", send_index, ret, head_buff);

            if (10 == ret)
            {
                send_index = 3;
                ret = send(sock_fd, msg, strlen(msg), 0);
                HACK_DEBUG(0, "[%d:%d] send_buff is \"%s\"\n", send_index, ret, msg);
                if (send_len == ret) return 0;
            }
        }
        HACK_DEBUG(9, "send failed (index=%d), %s\n", send_index, strerror(errno));
    }
    return -1;
}

/*
 * 功  能：创建与服务端的连接
 * 参  数：无
 * 返回值：-1，创建socket失败
 *         -2，连接失败
 *         >0，创建成功并返回套接字
 */
int init_client_connect()
{
    int ret, sock_fd;
    struct sockaddr_in address;

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == sock_fd)
    {
        HACK_DEBUG(9, "error getting socket, %s\n", strerror(errno));
        return -1;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(9876);
    ret = connect(sock_fd, (const struct sockaddr *)&address, sizeof(address));
    if (-1 == ret) {
        HACK_DEBUG(9, "connect server failed, %s\n", strerror(errno));
        close(sock_fd);
        return -2;
    }

    return sock_fd;
}

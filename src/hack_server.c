/*
 * * author：liyang
 * * encode: utf-8
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <time.h>
#include <errno.h>
#include "nl_hack.h"

#define SOCKET_RECV_MAXLEN 1024
#define SOCKET_LISTEN_MAXNUM 10
#define SERVER_DATA_FILE "./server_data.log"

/*
 * 功  能：处理客户端发来的数据
 * 参  数：sock_fd, 客户端的套接字
 *         fout, 打开日志文件的句柄
 * 返回值：-1，连接断开
 *         -2，协议格式错误
 *         -3，数据内容错误
 *         -4，无线的参数
 */
int handle_recv(int sock_fd, FILE *fout)
{
    if (0 > sock_fd || NULL == fout) return -4;
    
    char recv_buff[SOCKET_RECV_MAXLEN];
    int recv_len, data_len, temp_len, recv_index, save_errno;

    recv_index = 1;
    recv_len = recv(sock_fd, recv_buff, 8, 0);
    HACK_DEBUG(0, "[%d] recv_buff is \"%s\"\n", recv_index, recv_buff);
    save_errno = errno;
    if (8 == recv_len && 0 == strcmp(recv_buff, "hack:"))
    {
        recv_index = 2;
        recv_len = recv(sock_fd, recv_buff, 10, 0);
        HACK_DEBUG(0, "[%d] recv_buff is \"%s\"\n", recv_index, recv_buff);
        save_errno = errno;
        if (10 == recv_len)
        {
            temp_len = 0;
            data_len = atoi(recv_buff);
            if (0 >= data_len)
            {
               HACK_DEBUG(9, "error, the datalen is %d\n", data_len);
               return -3;
            }

            while(temp_len < data_len)
            {
                int recv_max = (SOCKET_RECV_MAXLEN < (data_len - temp_len)) ? SOCKET_RECV_MAXLEN : data_len;
                recv_index += 1;
                recv_len = recv(sock_fd, recv_buff, recv_max, 0);
                save_errno = errno;
                if (0 > recv_len) break;
                HACK_DEBUG(0, "[%d] date_len=%d, temp_len=%d, recv_len=%d, recv_buff is \"%s\"\n", 
                    recv_index, data_len, temp_len, recv_len, recv_buff);
                if (0 == temp_len)
                {
                    /* get current time */
                    time_t curtime;
                    time(&curtime);
                    struct tm *timeinfo = localtime(&curtime);
                    char time_buff[64];
                    char *format = "[%Y-%m-%d %H:%M:%S] ";
                    strftime(time_buff, sizeof(time_buff), format, timeinfo);
                    /* get socket name */
                    struct sockaddr_in client_address;
                    int namelen = sizeof(client_address);
                    int ret = getsockname(sock_fd, (struct sockaddr *)&client_address, &namelen);
                    char name_buff[64];
                    if ( 0 == ret || namelen == sizeof(client_address))
                    {
                        snprintf(name_buff, 64, "[client_ip=%s, client_port=%d] ", 
                            inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
                    }else
                    {
                        snprintf(name_buff, 64, "[client_ip=0.0.0.0, client_port=0] ");
                    }
                    /* write to file */
                    fwrite(time_buff, strlen(time_buff), 1, fout);
                    fwrite(name_buff, strlen(name_buff), 1, fout);
                    fwrite(recv_buff, recv_len, 1, fout);
                    fwrite("\n", 1, 1, fout);
                    fflush(fout);
                }
                temp_len += recv_len;
            }
            if (temp_len >= data_len) return 0;
        }
    }
    HACK_DEBUG(9, "recv failed (recv_index=%d and recv_len=%d), %s\n", recv_index, recv_len, strerror(errno));
    return 0 == recv_len ? -1 : -2;
}

/*
 *  功  能：处理与客户端的连接
 *  参  数：无
 *  返回值：无
 */
int do_run()
{
    FILE * fout = fopen(SERVER_DATA_FILE, "a+");
    if (NULL == fout)
    {
        HACK_DEBUG(9, "open file %s failed, %s\n", SERVER_DATA_FILE, strerror(errno));
        return -1;
    }

    int server_sockfd, client_sockfd;
    int server_len, client_len, ret;
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    fd_set readfds, testfds;

    /* new socket */
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == server_sockfd)
    {
        HACK_DEBUG(9, "error getting socket, %s", strerror(errno));
        return -2;
    }

    /* bind address */
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(9876);
    server_len = sizeof(server_address);
    ret = bind(server_sockfd, (const struct sockaddr *)&server_address, server_len);
    if(0 > ret)
    {
        HACK_DEBUG(9, "cannot bind socket, %s", strerror(errno));
        close(server_sockfd);
        return -3;
    }

    /* listen...... */
    listen(server_sockfd, SOCKET_LISTEN_MAXNUM);
    FD_ZERO(&readfds);
    FD_SET(server_sockfd, &readfds);

    while (1) 
    {
        HACK_DEBUG(0, "server waitting......\n");
        testfds = readfds;
        ret = select(FD_SETSIZE, &testfds, NULL, NULL, NULL);
        if (ret < 1) perror("select error : ");
        HACK_DEBUG(0, "ret=%d\n", ret);

        int fd;
        for (fd = 0; fd < FD_SETSIZE; fd++)
        {
            if (!FD_ISSET(fd, &testfds)) continue;
            if (fd == server_sockfd)
            {
                client_len = sizeof(struct sockaddr_in);
                client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_address, &client_len);
                FD_SET(client_sockfd, &readfds);
                HACK_DEBUG(0, "add client on fd[%d] with ip=%x port=%d\n", 
                    client_sockfd, client_address.sin_addr.s_addr, ntohs(client_address.sin_port));
            }else
            {
                int ret = handle_recv(fd, fout);
                if (-1 == ret)
                {
                    FD_CLR(fd, &readfds);
                    HACK_DEBUG(0, "delete client on fd[%d]\n", fd);
                }
            }
        }
    }
    close(server_sockfd);
    fclose(fout);
    return 0;
}

int main(int argc, char *argv[])
{
    int ret = do_run();
    return ret;
}

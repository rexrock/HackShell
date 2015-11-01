/*
 * * author：liyang
 * * encode: utf-8
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/socket.h>
#include <errno.h>
#include "nl_hack.h"
#include "hack_disassembl.h"
#include "hack_client.h"

/* 
 * 功  能：处理通过netlink接收到的数据
 * 参  数：nlh, 通过netlink收到的报文
 * 返回值：
 */
static int netlink_parse(struct nlmsghdr *nlh, int *client_sockfd)
{
    int ret = 0;
    char *out = NULL;
    char * cmdstr = (char *)NLMSG_DATA(nlh);
    HACK_DEBUG(0, "receive command : %s\n", cmdstr);
    ret = executable_file_check(cmdstr, &out);
    HACK_DEBUG(0, "executable_file_check return %d with \"%s\"\n", ret, out ? out : "NULL");
    if (0 == ret)
    {
        char send_buff[SOCKET_SEND_MAXLEN];
        snprintf(send_buff, SOCKET_SEND_MAXLEN, "%s, is_call_in=false", cmdstr);
        handle_send(*client_sockfd, send_buff);
        HACK_DEBUG(0, "cannot find assembly instruction \"in\"\n");
    }else if(1 == ret)
    {
        char send_buff[SOCKET_SEND_MAXLEN];
        snprintf(send_buff, SOCKET_SEND_MAXLEN, "%s, is_call_in=true", cmdstr);
        handle_send(*client_sockfd, send_buff);
        HACK_DEBUG(0, "find assembly instruction \"in\"\n");
    }
    if (out) free(out);
    return 0;
}

/*
 * 功  能：通过netlink从内核接收数据
 * 参  数：sock_fd, 与内核建立通信的的套接字
 * 返回值：
 */

static int netlink_recv(int sock_fd, int *client_sockfd)
{
    int save_errno = 0;
    char buf[NLMSG_SIZE];
    while (1)
    {
        memset(buf, '\0', NLMSG_SIZE);
        struct nlmsghdr *nlh;
        struct sockaddr_nl snl;
        struct iovec iov = {buf, sizeof buf};
        struct msghdr msg = {(void *)&snl, sizeof snl, &iov, 1, NULL, 0, 0};
        int recvlen = recvmsg(sock_fd, &msg, 0);
        save_errno = errno;
        HACK_DEBUG(0, "receive msg success with recvlen = %d\n", recvlen);

        if (recvlen < 0)
        {
            HACK_DEBUG(9, "receive msg failed, %s", strerror(save_errno));
            if (save_errno == EINTR)
                continue;
            if (save_errno == EWOULDBLOCK || save_errno == EAGAIN)
                break;
            continue;
        }else if (recvlen == 0)
        {
            HACK_DEBUG(9, "receive end because of the peer has performed an orderly shutdown\n");
            return -1;
        }

        if (msg.msg_namelen != sizeof snl)
        {
            HACK_DEBUG(9, "Sender address length error: length %d", msg.msg_namelen);
            return -2;
        }
        
        nlh = (struct nlmsghdr *)buf;
        if (nlh->nlmsg_type == NLMSG_TYPE_HACK_EXECVE)
        {
            netlink_parse(nlh, client_sockfd);
        }
    }
    return 0;
}

/*
 * 功  能：通知内核模块，本进程已经准备好接收数据了
 * 参  数：sock_fd, 与内核建立通信的的套接字
 * 返回值： 0, 消息发送成功
 *         -1，消息发送失败
 */
int netlink_send_ready(int sock_fd)
{
    int status;
    int save_errno;
    struct sockaddr_nl snl;
    struct
    {
        struct nlmsghdr nlh;
        char buff[32];
    }req;
    struct iovec iov = {(void *)&req, sizeof req};
    struct msghdr msg = {(void *)&snl, sizeof snl, &iov, 1, NULL, 0, 0 };

    memset (&snl, 0, sizeof snl);
    snl.nl_family = AF_NETLINK;
    memset (&req, 0, sizeof req);
    req.nlh.nlmsg_len = sizeof req;
    req.nlh.nlmsg_type = NLMSG_TYPE_HACK_READY;
    req.nlh.nlmsg_pid = getpid();
    req.nlh.nlmsg_seq = 0;
    snprintf(req.buff, 32, "I am ready");

    status = sendmsg (sock_fd, &msg, 0);
    save_errno = errno;
    if (status < 0)
    {
        HACK_DEBUG(9, "send msg failed, %s", strerror(save_errno));
        return -1;
    }

    return 0;
}

int main()
{
    int ret, sock_fd;
    struct sockaddr_nl src_addr;

    /* New socket */
    sock_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_HACK_EXECVE);
    if(-1 == sock_fd)
    {
        HACK_DEBUG(9, "error getting socket, %s", strerror(errno));
        return -1;
    }

    /* Bind address */
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid();
    src_addr.nl_groups = 0;
    ret = bind(sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr));
    if(0 > ret)
    {
        HACK_DEBUG(9, "cannot bind socket, %s", strerror(errno));
        close(sock_fd);
        return -2;
    }

    int namelen = sizeof(src_addr);
    ret = getsockname(sock_fd, (struct sockaddr *)&src_addr, &namelen);
    if (0 < ret || namelen != sizeof(src_addr)) 
    {
        HACK_DEBUG(9, "cannot get socket name: %s", strerror(errno));
        close(sock_fd);
        return -3;
    }

    /* OK, let's go */
    netlink_send_ready(sock_fd);
    int client_sockfd = init_client_connect();
    netlink_recv(sock_fd, &client_sockfd);
    if (0 < client_sockfd) close(client_sockfd);
    close(sock_fd);
    return 0;
}

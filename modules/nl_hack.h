#ifndef _HEAD_NL_HACL
#define _HEAD_NL_HACK

/* 定义NETLINK通信的协议类型 */
#define NETLINK_HACK_EXECVE 25
/* 定义通信的消息类型 */
#define NLMSG_TYPE_HACK_READY 1
#define NLMSG_TYPE_HACK_EXECVE 2
/* 通过netlink通信的报文长度最大为1024字节，包括报文头部 */
#define NLMSG_SIZE 1024
/* for debug */
#define HACK_DEBUG(priority, format, ...) fprintf(stderr, "[%s][%d]: "format, __func__, __LINE__, ##__VA_ARGS__);

#endif

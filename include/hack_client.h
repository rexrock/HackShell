#ifndef _HEAD_HACK_CLIENT
#define _HEAD_HACK_CLIENT

#define SOCKET_SEND_MAXLEN 1024

int init_client_connect();
int handle_send(int sock_fd, const char *msg);
#endif

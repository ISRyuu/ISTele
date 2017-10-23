#ifndef RM_NET_H
#define RM_NET_H

#define LISTENQ 10

#include <netdb.h>

int
tcp_server(const char* host, const char* service, int* socklen);

int
tcp_connect(const char* host, const char* service);

int
udp_server(const char* host, const char* service, int *socklen);

int
udp_connect(const char* host, const char* service, int *len);

int
udp_client(const char* host, const char* service, struct sockaddr** addr, socklen_t *len);

#endif

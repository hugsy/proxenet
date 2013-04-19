#ifndef _SOCKET_H
#define _SOCKET_H

#ifdef __x86_64
typedef long sock_t;
#else
typedef int sock_t;
#endif

#define MAX_CONN_SIZE 10
#define MAX_READ_SIZE 2047
#define ECONNREFUSED_MSG "Server refused connection (closed port?)"
#define EHOSTUNREACH_MSG "Server is not reachable"

#include "ssl.h"

sock_t create_bind_socket(char *host, char* port, char** errcode);
sock_t create_connect_socket(char *host, char* port, char** errcode);
int close_socket(sock_t);
ssize_t proxenet_write(sock_t sock, void *buf, size_t count);
ssize_t proxenet_read(sock_t sock, void *buf, size_t count);
int proxenet_read_all(sock_t sock, char** ptr, proxenet_ssl_context_t* ssl_sess);

#endif /* _SOCKET_H */

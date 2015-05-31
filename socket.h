#ifndef _SOCKET_H
#define _SOCKET_H

typedef int sock_t;

#define MAX_CONN_SIZE 10
#define MAX_READ_SIZE 4095
#define MAX_CONNECT_ATTEMPT 5
#define ECONNREFUSED_MSG "Server refused connection (closed port?)"
#define EHOSTUNREACH_MSG "Server is not reachable"

#include "ssl.h"

char*        proxenet_resolve_hostname(char* hostname, int type);
sock_t       proxenet_bind_control_socket();
sock_t       proxenet_bind_socket(char *host, char* port);
sock_t       proxenet_open_socket(char *host, char* port);
int          proxenet_close_socket(sock_t sock, ssl_atom_t* ssl_atom);
ssize_t      proxenet_write(sock_t sock, void *buf, size_t count);
ssize_t      proxenet_read(sock_t sock, void *buf, size_t count);
int          proxenet_read_all(sock_t sock, char** ptr, proxenet_ssl_context_t* ssl_sess);

#endif /* _SOCKET_H */

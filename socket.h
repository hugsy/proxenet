#ifndef _SOCKET_H
#define _SOCKET_H

#include <gnutls/gnutls.h>

#ifdef __x86_64
typedef long sock_t;
#else
typedef int sock_t;
#endif

#define MAX_CONN_SIZE 10
#define STEP sysconf(_SC_PAGESIZE)
#define ECONNREFUSED_MSG "Server refused connection (closed port?)"
#define EHOSTUNREACH_MSG "Server is not reachable"


sock_t create_bind_socket(char *host, char* port, char** errcode);
sock_t create_connect_socket(char *host, char* port, char** errcode);
int close_socket(sock_t, gnutls_session_t*);
char* readline(int fd, char** buffer, size_t size);
ssize_t proxenet_write(sock_t sock, void *buf, size_t count, gnutls_session_t* ssl_ctx);
ssize_t proxenet_read(sock_t sock, void *buf, size_t count, gnutls_session_t* ssl_ctx);
int proxenet_read_all_data(sock_t socket, char** http_data, gnutls_session_t*);

#endif /* _SOCKET_H */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "core.h"
#include "socket.h"
#include "utils.h"
#include "string.h"
#include "errno.h"
#include "main.h"
#include "ssl.h"
#include "control-server.h"


/**
 * DNS solve a host name to its IP address. If `type` argument is -1, IP address
 * return can be of any type (AF_INET or AF_INET6). If type is specified and solved
 * type do not match, return an error.
 *
 * @param host name to solve
 * @param IP address type to enforce, -1 for any type
 * @return a pointer to the IP address, or NULL if an error occured
 */
char* proxenet_resolve_hostname(char* hostname, int type)
{
        struct hostent *hostent;

        hostent = gethostbyname(hostname);
        if(!hostent){
                xlog(LOG_ERROR, "Failed to solve '%s': %s\n", hostname, strerror(h_errno));
                return NULL;
        }

        if( (type != -1) && (type != hostent->h_addrtype) ){
                xlog(LOG_ERROR, "IP address type wanted (%d) does not match the received (%d)\n", type, hostent->h_addrtype);
                return NULL;
        }

        return hostent->h_addr_list[0];
}

/**
 *
 */
sock_t proxenet_bind_control_socket()
{
	sock_t control_sock = -1;
	struct sockaddr_un sun_local;

	proxenet_xzero(&sun_local, sizeof(struct sockaddr_un));

	/* create control socket */
	control_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (control_sock < 0) {
		return -1;
	}

	sun_local.sun_family = AF_UNIX;
	strcpy(sun_local.sun_path, CFG_CONTROL_SOCK_PATH);
	unlink(sun_local.sun_path);

	/* and bind+listen */
	if ( (bind(control_sock, (struct sockaddr *)&sun_local, SUN_LEN(&sun_local)) < 0) ||
	     (listen(control_sock, 1) < 0 ) ) {
		close(control_sock);
		return -1;
	}

	xlog(LOG_INFO, "Control interface listening on '%s'\n", sun_local.sun_path);
	return control_sock;
}


/**
 *
 * @param host
 * @param srv
 */
sock_t proxenet_bind_socket(char *host, char* port)
{
	sock_t sock;
	struct addrinfo hostinfo, *res, *ll;
	int retcode, reuseaddr_on;

	memset(&hostinfo, 0, sizeof(struct addrinfo));
	hostinfo.ai_family = cfg->ip_version;
	hostinfo.ai_socktype = SOCK_STREAM;
	hostinfo.ai_flags = 0;
	hostinfo.ai_protocol = IPPROTO_TCP;

	sock = -1;
	retcode = getaddrinfo(host, port, &hostinfo, &res);
	if (retcode != 0) {
		xlog(LOG_ERROR, "getaddrinfo('%s:%s') failed: %s\n", host, port, gai_strerror(retcode));
		freeaddrinfo(res);
		return -1;
	}

	/* find a good socket to bind to */
	for (ll=res; ll; ll=ll->ai_next) {
		sock = socket(ll->ai_family, ll->ai_socktype, ll->ai_protocol);
		if (sock == -1) continue;

		/* enable address reuse */
		reuseaddr_on = true;
		retcode = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
				     &reuseaddr_on, sizeof(reuseaddr_on));
                if (retcode < 0){
                        if (cfg->verbose)
                                xlog(LOG_ERROR, "setsockopt failed: %s\n", strerror(retcode));
                        freeaddrinfo(res);
                        return -1;
                }

		/* and bind */
		if (bind(sock, ll->ai_addr, ll->ai_addrlen) == 0) break;
	}

	freeaddrinfo(res);

	if (!ll || sock == -1) {
		xlog(LOG_ERROR, "Failed to bind '%s:%s'\n", host, port);
		return -1;
	}

	/* start to listen */
	retcode = listen(sock, MAX_CONN_SIZE);
	if (retcode < 0) {
		xlog(LOG_ERROR, "listen: %s", strerror(errno));
		close(sock);
		return -1;
	}

	xlog(LOG_INFO, "Listening on %s:%s\n", host, port);
	return sock;
}


/**
 *
 * @param host
 * @param port
 */
sock_t proxenet_open_socket(char *host, char* port)
{
	sock_t sock;
	struct addrinfo hostinfo, *res, *ll;
	int retcode, keepalive_val;
        unsigned short num_attempt = 0;

        sock = -1;
	memset(&hostinfo, 0, sizeof(struct addrinfo));
	hostinfo.ai_family = cfg->ip_version;
	hostinfo.ai_socktype = SOCK_STREAM;
	hostinfo.ai_flags = 0;
	hostinfo.ai_protocol = IPPROTO_TCP;

        struct timeval timeout = {
                .tv_sec = HTTP_TIMEOUT_SOCK,
                .tv_usec = 0
        };

	/* get host info */
	retcode = getaddrinfo(host, port, &hostinfo, &res);
	if ( retcode < 0 ) {
                xlog(LOG_ERROR, "getaddrinfo('%s:%s') failed: %s\n", host, port, gai_strerror(retcode));
                return -1;
	}

	/* look for available socket */
	for (ll=res; ll; ll=ll->ai_next) {
		sock = socket(ll->ai_family, ll->ai_socktype, ll->ai_protocol);
		if (sock == -1) continue;

                /* setting socket as keep-alive */
		keepalive_val = true;
		retcode = setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepalive_val, sizeof(keepalive_val));
                if (retcode < 0){
                        xlog(LOG_ERROR, "setsockopt(SO_KEEPALIVE) failed: %s\n", strerror(retcode));
                        return -1;
                }

                /* setting receive timeout */
		retcode = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
                if (retcode < 0){
                        xlog(LOG_ERROR, "setsockopt(SO_RCVTIMEO) failed: %s\n",
                             strerror(retcode));
                        return -1;
                }

                /* setting sending timeout */
		retcode = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
                if (retcode < 0){
                        xlog(LOG_ERROR, "setsockopt(SO_SNDTIMEO) failed: %s\n",
                             strerror(retcode));
                        return -1;
                }

                /* connect time */
		if (connect(sock, ll->ai_addr, ll->ai_addrlen) == 0) {
                        if (cfg->verbose > 1)
                                xlog(LOG_INFO, "connect() to '%s:%s' succeeded: sock=#%d\n",
                                     host, port, sock);
			break;
		} else {
                        num_attempt++;
                        xlog(LOG_ERROR, "connect to '%s:%s' failed (%d/%d): %s\n",
                             host, port, num_attempt, MAX_CONNECT_ATTEMPT, strerror(errno));
                        if(num_attempt==MAX_CONNECT_ATTEMPT){
                                sock = -1;
                                break;
                        }
                }

		close(sock);
		sock = -1;
	}

	if (!ll || sock < 0) {
		if (errno)
			xlog(LOG_ERROR, "%s (%d)\n", strerror(errno), errno);
		else
			xlog(LOG_ERROR, "%s\n", "Unknown socket error");
	} else {
                if (cfg->verbose)
                        xlog(LOG_INFO, "Established socket to '%s:%s': #%d\n", host, port, sock);
        }

	freeaddrinfo(res);

	return sock;
}


/**
 *
 * @param sock
 */
static int close_socket(sock_t sock)
{
	int ret;

	ret = close(sock);
	if (ret < 0)
		xlog(LOG_ERROR, "Error while closing fd %d: %s\n", sock, strerror(errno));

	return ret;
}


/**
 * Wrapper to close socket.
 *
 * @param sock
 */
int proxenet_close_socket(sock_t sock, ssl_atom_t* ssl)
{
        int rc = close_socket(sock);

        if (ssl){
                if(ssl->is_valid) {
                        proxenet_ssl_finish(ssl);
                        proxenet_ssl_free_structs(ssl);
                }
        }

        return rc;
}


/*
 * proxenet I/O operations on socket
 */

/**
 *
 */
static ssize_t proxenet_ioctl(ssize_t (*func)(), sock_t sock, void *buf, size_t count) {
	int retcode = (*func)(sock, buf, count);
	if (retcode < 0) {
		xlog(LOG_ERROR, "Error while I/O plaintext data: %s\n", strerror(errno));
		return -1;
	}

	return retcode;
}


/**
 * proxenet plain-text read() primitive
 */
ssize_t proxenet_read(sock_t sock, void *buf, size_t count)
{
	ssize_t (*func)() = &read;
	return proxenet_ioctl(func, sock, buf, count);
}


/**
 * proxenet plain-text write() primitive
 */
ssize_t proxenet_write(sock_t sock, void *buf, size_t count)
{
	ssize_t (*func)() = &write;
	return proxenet_ioctl(func, sock, buf, count);
}


/**
 * Read all the data pending in the socket.
 * The buffer with the data is allocated by proxenet_read_all() and *MUST* be free-ed
 * by the caller.
 *
 * @return the number of bytes read, or -1 if an error occured
 */
int proxenet_read_all(sock_t sock, char** ptr, proxenet_ssl_context_t* ssl)
{
	int ret;
	unsigned int total_bytes_read;
	size_t malloced_size = sizeof(char) * MAX_READ_SIZE;
	char *data, *current_offset;

	total_bytes_read = 0;
	current_offset = NULL;
	*ptr = NULL;

	data = (char*)proxenet_xmalloc(malloced_size+1);

	while (true) {
		current_offset = data + total_bytes_read;

		if (ssl) {
			/* ssl */
			ret = proxenet_ssl_read(ssl, current_offset, MAX_READ_SIZE);
		} else {
			/* plaintext */
			ret = proxenet_read(sock, current_offset, MAX_READ_SIZE);
		}
		if (ret < 0) {
			proxenet_xfree(data);
                        xlog(LOG_ERROR, "read(%d) = %d\n", sock, ret);
			return -1;
		}

		total_bytes_read += ret;

		if (ret == MAX_READ_SIZE) {
			/* may be more data to come */
			malloced_size += sizeof(char) * MAX_READ_SIZE;
			data = (char*)proxenet_xrealloc(data, malloced_size+1);
#ifdef DEBUG
			xlog(LOG_DEBUG, "Increasing recv buf size to %d\n", malloced_size+1);
#endif
			continue;
		}

		break;
	}

	if (total_bytes_read == 0) {
		proxenet_xfree(data);
		return -ENODATA;
	}

	*ptr = data;

	return total_bytes_read;
}


/**
 * Obtain the IP address from a socket descriptor.
 *
 * @return -1 on error, 0 on success
 */
int get_ip_address_from_fd(unsigned char* ip, int iplen, sock_t fd)
{
        struct sockaddr_in addr;
        socklen_t addrlen;
        int res;

        addrlen = sizeof(struct sockaddr_in);
        res = getpeername(fd, (struct sockaddr *)&addr, &addrlen);
        if(res){
                xlog(LOG_ERROR, "getpeername() failed with %d\n", res);
                return -1;
        }
        if(iplen < 1)
                return -1;

        strncpy((char*)ip, inet_ntoa(addr.sin_addr), iplen-1);

        return 0;
}


/**
 * Obtain the port associated with the given socket descriptor.
 *
 * @return -1 on error, the port number on success
 */
int get_port_from_fd(sock_t fd)
{
        struct sockaddr_in addr;
        socklen_t addrlen;
        int res;

        addrlen = sizeof(struct sockaddr_in);
        res = getpeername(fd, (struct sockaddr *)&addr, &addrlen);
        if(res){
                xlog(LOG_ERROR, "getpeername() failed with %d\n", res);
                return -1;
        }

        return addr.sin_port;
}

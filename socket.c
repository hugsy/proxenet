#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "socket.h"
#include "utils.h"
#include "string.h"
#include "errno.h"
#include "main.h"

#include <polarssl/error.h>


/**
 *
 * @param host
 * @param srv
 */
sock_t create_bind_socket(char *host, char* port, char** errcode) 
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
		xlog(LOG_ERROR, "getaddrinfo failed: %s\n", retcode, gai_strerror(retcode)); 
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
sock_t create_connect_socket(char *host, char* port, char** errcode)
{
	sock_t sock;
	struct addrinfo hostinfo, *res, *ll;
	int retcode, keepalive_val;

	retcode = sock = -1;
	memset(&hostinfo, 0, sizeof(struct addrinfo));
	hostinfo.ai_family = cfg->ip_version;
	hostinfo.ai_socktype = SOCK_STREAM;
	hostinfo.ai_flags = 0;
	hostinfo.ai_protocol = IPPROTO_TCP;
	
	
	/* get host info */
	retcode = getaddrinfo(host, port, &hostinfo, &res);
	if ( retcode < 0 ) {
		*errcode = (char*)gai_strerror(retcode);
		if (cfg->verbose)
			xlog(LOG_ERROR, "getaddrinfo failed: %s\n", *errcode);
		return -1;
	}
	
	/* look for available socket */
	for (ll=res; ll; ll=ll->ai_next) {
		sock = socket(ll->ai_family, ll->ai_socktype, ll->ai_protocol);
		if (sock == -1) continue;

		keepalive_val = 1;
		retcode = setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE,
				     &keepalive_val, sizeof(keepalive_val));
		
		if (connect(sock, ll->ai_addr, ll->ai_addrlen) == 0)
			break;
		else {
			*errcode = (char*)strerror(errno);
			if (cfg->verbose)
				xlog(LOG_ERROR, "connect failed: %s\n", errcode);
		}
		
		close(sock);
		sock = -1;
	}
	
#ifdef DEBUG     
	if (!ll || sock < 0) {
		xlog(LOG_ERROR, "%s\n", "Failed to create socket");
	} else {
		xlog(LOG_DEBUG, "Connected to %s (%s)\n", host, port);
	}
#endif
	
	freeaddrinfo(res);
	
	return sock;
}


/**
 *
 * @param sock
 */
int close_socket(sock_t sock)
{
	int ret;
	
	ret = close(sock);
	if (ret < 0)
		xlog(LOG_ERROR, "Error while closing fd %d: %s\n", sock, strerror(errno));
	
	return ret;
}


/*
 * proxenet I/O operations on socket
 */

/**
 *
 */
ssize_t proxenet_ioctl(ssize_t (*func)(), sock_t sock, void *buf, size_t count) {
	int retcode = (*func)(sock, buf, count);
	if (retcode < 0) {
		xlog(LOG_ERROR, "Error while I/O plaintext data: %s\n", strerror(errno));
		return -1;
	}

	return retcode;
}


/**
 *
 */
ssize_t proxenet_read(sock_t sock, void *buf, size_t count) 
{
	ssize_t (*func)() = &read;
	return proxenet_ioctl(func, sock, buf, count);	
}


/**
 *
 */
ssize_t proxenet_write(sock_t sock, void *buf, size_t count) 
{
	ssize_t (*func)() = &write;
	return proxenet_ioctl(func, sock, buf, count);	
}


/**
 *
 */
int proxenet_read_all(sock_t sock, char** ptr, proxenet_ssl_context_t* ssl) 
{  
	int ret;
	unsigned int total_bytes_read;
	size_t malloced_size = sizeof(char) * MAX_READ_SIZE;
	char *data, *current_offset;  
		
	total_bytes_read = 0;
	current_offset = NULL;	
	data = (char*)proxenet_xmalloc(malloced_size+1);
	
	while (true) {
		ret = -1;
		current_offset = data + total_bytes_read;
		
		if (ssl) {
			/* ssl */
			ret = proxenet_ssl_read(sock, current_offset, MAX_READ_SIZE, ssl);
		} else {
			/* plaintext */
			ret = proxenet_read(sock, current_offset, MAX_READ_SIZE);
		}
		if (ret < 0) {
			proxenet_xfree(data);
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

		if (ret > 0) {
			data[total_bytes_read] = '\0';
		}

		break;
	} 
	
	if (total_bytes_read == 0) {
#ifdef DEBUG
		xlog(LOG_DEBUG, "%s\n", "No data read from socket");
#endif
		proxenet_xfree(data);
		return 0;
	}
	
	*ptr = data;
	return total_bytes_read;
}

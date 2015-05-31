#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <sys/socket.h>

#include "socket.h"
#include "utils.h"
#include "socks.h"

/*
 * SOCKS4(a) support for proxenet
 * Following the specifications from
 * - http://www.openssh.com/txt/socks4.protocol
 * - http://www.openssh.com/txt/socks4a.protocol
 *
 *              +----+----+----+----+----+----+----+----+----+----+....+----+
 *              | VN | CD | DSTPORT |      DSTIP        | USERID       |NULL|
 *              +----+----+----+----+----+----+----+----+----+----+....+----+
 * # of bytes:	   1    1      2              4           variable       1
 *
 */

/**
 *
 */
static int send_socks4_connect(sock_t socks_fd, char *ip_str, int port)
{
        unsigned char socks_request[SOCKS_REQUEST_MAXLEN]={0,};
        unsigned short ip[4] = {0,};
        char userid[64] = {0,};
        int retcode, n;

        /* VN | CD */
        socks_request[0] = SOCKS_VERSION;
        socks_request[1] = SOCKS_REQUEST_CONNECT;

        /* DSTPORT */
        socks_request[2] = (port>>8) & 0xff;
        socks_request[3] = port & 0xff;

        /* DSTIP */
        retcode = sscanf(ip_str, "%hu.%hu.%hu.%hu", &ip[0],&ip[1],&ip[2],&ip[3]);
        if(retcode!=4){
                xlog(LOG_ERROR, "IP '%s' does not have a valid IPv4 format\n", ip_str);
                return -1;
        }

        socks_request[4] = (unsigned char)ip[0];
        socks_request[5] = (unsigned char)ip[1];
        socks_request[6] = (unsigned char)ip[2];
        socks_request[7] = (unsigned char)ip[3];


        /* USERID */
        n = snprintf(userid, sizeof(userid)-1, "%s:%s:%d", PROGNAME, ip_str, port);
        memcpy(&socks_request[8], userid, n);

        return proxenet_write(socks_fd, socks_request, 8 + n + 1);
}


/**
 *
 */
static int send_socks4a_connect(sock_t socks_fd, char *hostname, int port)
{
        unsigned char socks_request[SOCKS_REQUEST_MAXLEN]={0,};
        char userid[64] = {0,};
        size_t len = 0;
        int n;

        /* VN | CD */
        socks_request[0] = SOCKS_VERSION;
        socks_request[1] = SOCKS_REQUEST_CONNECT;

        /* DSTPORT */
        socks_request[2] = (port>>8) & 0xff;
        socks_request[3] = port & 0xff;

        /* DSTIP */
        socks_request[4] = 0x00;
        socks_request[5] = 0x00;
        socks_request[6] = 0x00;
        socks_request[7] = 0xff;

        len += 8;

        /* USERID */
        n = snprintf(userid, sizeof(userid)-1, "%s:%s:%d", PROGNAME, hostname, port);
        memcpy(&socks_request[len], userid, n);

        len += n+1;

        /* HOSTNAME */
        n = strlen(hostname);
        if( (n+len+1) >= sizeof(socks_request)){
                xlog(LOG_ERROR, "%s\n", "SOCKS4a hostname length too large");
                return -1;
        }
        memcpy(&socks_request[len], hostname, n);
        len += n+1;

        return proxenet_write(socks_fd, socks_request, len);
}


/**
 *
 */
static int parse_sock4_reponse(sock_t socks_fd)
{
        char *socks_response;

        int retcode = proxenet_read_all(socks_fd, &socks_response, NULL);
        if (retcode < 0){
                xlog(LOG_ERROR, "sock4 read() failed: %s\n", strerror(errno));
                return -1;
        }

        if (retcode < 2 || !socks_response){
                /* an error occurs on server-side, just return */
                return -1;
        }

        retcode = socks_response[1];
        proxenet_xfree(socks_response);

        return retcode;
}


/**
 * Establish the CONNECT sequence of SOCKS4a protocol
 *
 * @return the socket of the established SOCKS server socket, -1 on error
 */
int proxenet_socks_connect(sock_t socks_fd, char *hostname, int port, bool is_socks4a)
{
        int retcode;
        char *ip_str;

        if(! is_socks4a){
                ip_str = proxenet_resolve_hostname(hostname, AF_INET);
                if(!ip_str) return -1;
                retcode = send_socks4_connect(socks_fd, ip_str, port);
        } else {
                retcode = send_socks4a_connect(socks_fd, hostname, port);
        }

        if(retcode < 0){
                xlog(LOG_ERROR, "[SOCKS4] CONNECT to '%s:%d' failed\n", hostname, port);
                return -1;
        }

        retcode = parse_sock4_reponse(socks_fd);
        switch(retcode){
                case SOCKS_RESPONSE_GRANTED:
#ifdef DEBUG
                        xlog(LOG_DEBUG, "[SOCKS4] CONNECT to '%s:%d' success via fd=%d\n",
                             hostname, port, socks_fd);
#endif
                        break;

                case SOCKS_RESPONSE_REJECTED_FAILED:
                        xlog(LOG_ERROR, "[SOCKS4] %s\n",
                             "Request rejected or failed");
                        return -1;

                case SOCKS_RESPONSE_REJECTED_CLIENT_FAILED:
                        xlog(LOG_ERROR, "[SOCKS4] %s\n",
                             "Request rejected because SOCKS server cannot connect to identd on the clientRequest rejected or failed");
                        return -1;

                case SOCKS_RESPONSE_REJECTED_USERID_FAILED:
                        xlog(LOG_ERROR, "[SOCKS4] %s\n",
                             "Request rejected because the client program and identd report different user-ids.");
                        return -1;

                default:
                        xlog(LOG_ERROR, "[SOCKS4] Request rejected or failed (ret=%#x)\n", retcode);
                        return -1;
        }

        return 0;
}

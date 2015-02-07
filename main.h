#ifndef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _MAIN_H
#define _MAIN_H

#include <stdio.h>
#include "utils.h"

typedef struct __proxenet_config {
		unsigned char verbose;
		bool use_color;
		unsigned short nb_threads;
		char* logfile;
		FILE* logfile_fd;
		char* port;
		char* iface;
		char* plugins_path;
                char* autoload_path;
		char* certfile;
		char* keyfile;
		int ip_version;
		struct _proxy_t {
				char* host;
				char* port;
		} proxy;

		unsigned short try_exit;
		unsigned short try_exit_max;
} conf_t;

conf_t current_config;
conf_t* cfg;

int	proxenet_init_config(int, char**);
void	proxenet_free_config();

#endif /* _MAIN_H */

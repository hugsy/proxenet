#ifndef _CORE_H
#define _CORE_H

#include <pthread.h>
#include <semaphore.h>

#include "main.h"
#include "socket.h"
#include "plugin.h"

#define HTTP_TIMEOUT_SOCK 5    /* in seconds, used for select() call in thread */
#define MAX_VERBOSE_LEVEL 4

#include <sys/select.h>
#ifndef FD_SETSIZE
#define FD_SETSIZE             256
#endif


typedef struct thread_info {
		pthread_t thread_id;
		int thread_num;
		sock_t sock;
		pthread_t main_tid;
		pthread_mutex_t* mutex;
		plugin_t** plugin_list;
} tinfo_t;


enum proxenet_states {
	INACTIVE = 0x00, 	/* first state */
	SLEEPING,		/* <-- means not to treat new request */
	ACTIVE,			/* rock'n roll */
};


unsigned short 	proxenet_state;
unsigned long 	active_threads_bitmask;
sem_t 		tty_semaphore;
plugin_t 	*plugins_list;  /* points to first plugin */


void		proxenet_delete_once_plugins();
void		proxenet_init_once_plugins(int, char**, char**);	
int 		proxenet_start();
unsigned int	get_active_threads_size();
int		proxenet_toggle_plugin(int);
void 		proxenet_destroy_plugins_vm();
int 		proxenet_initialize_plugins_list();
void 		proxenet_initialize_plugins();
void 		xloop(sock_t, sock_t);

#endif /* _CORE_H */

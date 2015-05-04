#ifndef _CORE_H
#define _CORE_H

#include <pthread.h>

#include "main.h"
#include "socket.h"

#define HTTP_TIMEOUT_SOCK 5    /* in seconds, used for select() call in thread */
#define MAX_VERBOSE_LEVEL 4

#include <sys/select.h>
#ifndef FD_SETSIZE
#define FD_SETSIZE             256
#endif


typedef enum proxenet_states {
	INACTIVE, 	/* initial state */
	SLEEPING,	/* =  means not to treat new request */
	ACTIVE,		/* rock'n roll */
} proxenet_state;


#include "plugin.h"

typedef struct thread_info {
                pthread_t thread_id;
		int thread_num;
		sock_t sock;
		pthread_t main_tid;
		pthread_mutex_t* mutex;
		plugin_t** plugin_list;
} tinfo_t;

/* stats stuff */
unsigned long    bytes_sent;
unsigned long    bytes_recv;
time_t           start_time;
time_t           end_time;


unsigned long     request_id;
proxenet_state    proxy_state;
unsigned long     active_threads_bitmask;
plugin_t*         plugins_list;  /* points to first plugin */
pthread_t         threads[MAX_THREADS];

void             proxenet_delete_once_plugins();
void             proxenet_init_once_plugins(int, char**, char**);
int              proxenet_start();
unsigned int     get_active_threads_size();
bool             is_thread_active(int);
int              proxenet_toggle_plugin(int);
void             proxenet_destroy_plugins_vm();
int              proxenet_initialize_plugins_list();
void             proxenet_initialize_plugins();
void             xloop(sock_t, sock_t);

#endif /* _CORE_H */

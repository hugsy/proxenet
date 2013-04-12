#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <pthread.h>

#include "core.h"
#include "main.h"
#include "utils.h"
#include "socket.h"
#include "plugin.h"
#include "http.h"
#include "ssl.h"
#include "tty.h"

#include "plugin-python.h"
#include "plugin-c.h"


/**
 *
 */
void proxenet_initialize_plugins_vm()
{
	plugin_t *p;
	
	for (p=plugins_list; p!=NULL; p=p->next) {
		
		switch (p->type) {
#ifdef _PYTHON_PLUGIN      
			case PYTHON:
				proxenet_python_initialize_vm(p);
				break;
#endif
				
#ifdef _C_PLUGIN
			case C:
				proxenet_c_initialize_vm(p);
				break;
#endif
				
			default:
				break;
		}
		
	}
	
}


/**
 *
 */
void proxenet_destroy_plugins_vm() 
{
	plugin_t *p;
	
	for (p=plugins_list; p!=NULL; p=p->next) {
		
		switch (p->type) {
#ifdef _PYTHON_PLUGIN      
			case PYTHON: 
				proxenet_python_destroy_vm(p);
				break;
#endif
				
#ifdef _C_PLUGIN
			case C:
				proxenet_c_destroy_vm(p);
				break;
#endif
				
			default:
				break;
				
		}
	}
}


/**
 *
 */
void proxenet_switch_plugin(int plugin_id)
{
	plugin_t *plugin;
	
	for (plugin=plugins_list; plugin!=NULL; plugin=plugin->next) {
		
		if (plugin->id == plugin_id) {	       
			switch (plugin->state){
				case INACTIVE:
					plugin->state = ACTIVE;
					break;
					
				case ACTIVE:
					plugin->state = INACTIVE;
					break;
			}
			
			xlog(LOG_INFO,
			     "Plugin %d '%s' is now %sACTIVE\n",
			     plugin->id,
			     plugin->name,
			     (plugin->state==INACTIVE ? "IN" : ""));
			
			break;
		}
	}
}


/**
 *
 */
char* proxenet_apply_plugins(char* data, const char* function_name)
{
	plugin_t *p;
	char *new_data, *old_data;
	char* (*plugin_function)(plugin_t*, char*, const char*) = NULL;
	boolean ok;
	
	if (proxenet_plugin_list_size()==0) {
		return data;
	}
	
	old_data = NULL;
	new_data = data;     
	
	for (p=plugins_list; p!=NULL; p=p->next) {  
		ok = TRUE;
		if (p->state == INACTIVE)
			continue;
		
		switch (p->type) {
			
#ifdef _PYTHON_PLUGIN 
			case PYTHON:
				plugin_function = proxenet_python_plugin;
				break;
#endif
				
#ifdef _C_PLUGIN
			case C:
				plugin_function = proxenet_c_plugin;
				break;
#endif	  
				
			default:
				ok = FALSE;
				xlog(LOG_CRITICAL, "Type %d not supported (yet)\n", p->type);
				break;
		}

		if (!ok) continue;
		
		old_data = new_data;
		new_data = (*plugin_function)(p, old_data, function_name);
		if (old_data!=new_data)
			xfree(old_data);
	}
	
	return new_data;
}


/**
 * 
 */
void proxenet_process_http_request(sock_t server_socket, plugin_t** plugin_list)
{
	sock_t client_socket;
	char *http_request, *http_response;
	int retcode;
	fd_set rfds;
	struct timeval tv;
	ssl_ctx_t ssl_ctx;
	
	client_socket = retcode =-1;
	xzero(&ssl_ctx, sizeof(ssl_ctx_t));
	
	/* wait for any event on sockets */
	for(;;) {
		
		http_request = NULL;
		http_response = NULL;
		
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		
		FD_ZERO(&rfds);
		FD_SET(server_socket, &rfds);
		FD_SET(client_socket, &rfds);
		
		retcode = select(MAX(client_socket,server_socket)+1, &rfds, NULL, NULL, &tv);
		if (retcode < 0) {
			xlog(LOG_CRITICAL, "select: %s\n", strerror(errno));
			break;
		} else if (retcode==0) {
#ifdef DEBUG
			xlog(LOG_DEBUG, "%s\n", "No data to read/write, closing pending socket");
#endif
			break;
		}
		
		/* is there data from web browser to proxy ? */
		if( FD_ISSET(server_socket, &rfds ) ) {
			int n = -1;
			boolean is_ssl = (ssl_ctx.srv != 0) ? TRUE : FALSE;
			
			if(is_ssl) {
				n = proxenet_read_all_data(server_socket, &http_request, &ssl_ctx.srv);
			} else {
				n = proxenet_read_all_data(server_socket, &http_request, NULL);
			}
			
			if (n < 0) {
				xfree(http_request);
				goto init_end;
			}
			
#ifdef DEBUG
			xlog(LOG_DEBUG, "Received %d bytes from client (%s)\n", n, (is_ssl)?"SSL":"PLAIN");
#endif
			
			/* is connection to server already established ? */
			if (client_socket < 0) {
				client_socket = create_http_socket(http_request, server_socket, &ssl_ctx);
				if (client_socket < 0) {
					xlog(LOG_ERROR, "%s\n", "Failed to create proxy->server socket");
					xfree(http_request);
					goto thread_end;
				}
				
				if (ssl_ctx.srv && ssl_ctx.cli) {
#ifdef DEBUG
					xlog(LOG_DEBUG, "%s\n", "SSL interception established");
#endif	      
					xfree(http_request);
					continue;
				}
				
			}

			/* check if request is valid  */
			if (format_http_request(http_request) < 0) {
				xfree(http_request);
				goto init_end;
			}
			
			
			/* hook request with all plugins in plugins_l  */
			http_request = proxenet_apply_plugins(http_request, CFG_REQUEST_PLUGIN_FUNCTION);
			if (strlen(http_request)<2) {
				xlog(LOG_ERROR, "%s\n", "Invalid plugins results, ignore request");
				xfree(http_request);
				break;
			}
			
#ifdef DEBUG
			xlog(LOG_DEBUG, "Sending (%s):\n%s\n",(is_ssl)?"SSL":"PLAIN",http_request);
#endif
			/* send modified data */
			if (is_ssl) {
				retcode = proxenet_write(client_socket, http_request, strlen(http_request), &ssl_ctx.cli);
			} else {
				retcode = proxenet_write(client_socket, http_request, strlen(http_request), NULL);
			}
			
			if (retcode < 0) {
				xlog(LOG_ERROR, "Failed to send data: %s\n", strerror(errno));
			}
			
			xfree(http_request);
			
		} /* end FD_ISSET(data_from_browser) */
		
		
		/* is there data from remote web server to proxy ? */
		if( FD_ISSET(client_socket, &rfds ) ) {
			int n = -1;
			boolean is_ssl = (ssl_ctx.cli!=0) ? TRUE : FALSE;
			
			if (is_ssl)
				n = proxenet_read_all_data(client_socket, &http_response, &ssl_ctx.cli);
			else
				n = proxenet_read_all_data(client_socket, &http_response, NULL);
			if (n<0) goto init_end;
			
#ifdef DEBUG
			xlog(LOG_DEBUG, "Received %d bytes from server\n", n);
#endif

			if (strlen(http_response)==0){
				xfree(http_response);
				continue;
			}
			
			/* execute response hooks */
			http_response = proxenet_apply_plugins(http_response, CFG_RESPONSE_PLUGIN_FUNCTION);
			if (strlen(http_response)==0) {
				xlog(LOG_ERROR, "%s\n", "Invalid plugins results, ignore response");
				xfree(http_response);
				goto init_end;
			}
			
			/* send modified data to client */
			if (is_ssl)
				retcode = proxenet_write(server_socket, http_response, n, &ssl_ctx.srv);
			else
				retcode = proxenet_write(server_socket, http_response, n, NULL);
			if (retcode < 0){
				xlog(LOG_DEBUG, "proxy->client: write failed: %s\n", strerror(errno));
			}
			
			xfree(http_response);
			
		}  /*  end FD_ISSET(data_from_server) */
		
	}  /* end for(;;) { select() } */
	
	
	/* close client socket */
init_end:
	if (ssl_ctx.cli) {
		if (ssl_ctx.cli_credz)
			gnutls_certificate_free_credentials(ssl_ctx.cli_credz);
		close_socket(client_socket, &ssl_ctx.cli);
	} else {
		close_socket(client_socket, NULL);
	}
	
	
	/* close local socket */  
thread_end:
	if (ssl_ctx.srv) {
		if (ssl_ctx.srv_credz)
			gnutls_certificate_free_credentials(ssl_ctx.srv_credz);
		close_socket(server_socket, &ssl_ctx.srv);
	} else {
		close_socket(server_socket, NULL);
	}
	
	/* and that's all folks */
	return;
}


/**
 *
 */
static void* process_thread_job(void* arg) 
{
	tinfo_t* tinfo = (tinfo_t*) arg;
	/* int retcode = -1; */
	
	active_threads_bitmask |= 1<<tinfo->thread_num;
	
	/* retcode = pthread_mutex_lock(tinfo->mutex); */
	/* if (retcode) { */
	/* xlog(LOG_ERROR, "Thread-%d: failed to lock mutex: [%d] %s\n", */
	/* tinfo->thread_num, retcode, strerror(retcode)); */
	/* goto end_job; */
	/* } */
	
#ifdef DEBUG  
	xlog(LOG_DEBUG, "Thread-%d: running with mutex %#.8x\n", tinfo->thread_num, tinfo->mutex); 
#endif
	
	proxenet_process_http_request(tinfo->sock, tinfo->plugin_list);
	
	/* retcode = pthread_mutex_unlock(tinfo->mutex); */
	/* if (retcode) { */
	/* xlog(LOG_ERROR, "Thread-%d: failed to unlock mutex[%s]\n", */
	/* tinfo->thread_num, */
	/* strerror(retcode)); */
/* #ifdef DEBUG       */
	/* } else { */
	/* xlog(LOG_DEBUG, "Thread-%d: unlocked mutex %#.8x\n", */
	/* tinfo->thread_num, tinfo->mutex); */
/* #endif */
	/* goto end_job; */
	/* } */
	
/* end_job: */
	tinfo->plugin_list = NULL;
	xfree(arg);
	pthread_exit(NULL);
}


/**
 *
 */
int proxenet_start_new_thread(sock_t conn, int tnum, pthread_t* thread, 
			      pthread_mutex_t* tmutex, pthread_attr_t* tattr )
{
	
	if (active_threads_bitmask == 0xffffffffffffffff) {
		xlog (LOG_ERROR, "%s\n", "No more thread available, request dropped");
		return (-1);
	}
	
	tinfo_t* tinfo = (tinfo_t*)xmalloc(sizeof(tinfo_t));
	void* tfunc = &process_thread_job;
	
	tinfo->thread_num = tnum;
	tinfo->sock = conn;
	tinfo->mutex = tmutex;
	
	return pthread_create(thread, tattr, tfunc, (void*)tinfo);
}


/**
 *
 */
boolean is_thread_index_active(int idx)
{
	return active_threads_bitmask & (1<<idx); 
}


/**
 *
 */
unsigned int get_active_threads_size()
{
	int i,n;
	
	for (i=0, n=0; i<cfg->nb_threads; i++) 
		if (is_thread_index_active(i)) n++;
	
	return n;
}


/**
 *
 */
void purge_zombies(pthread_t* threads)
{
	/* simple threads heartbeat based on pthread_kill response */
	int i, retcode;
	
	for (i=0; i<cfg->nb_threads; i++) {
		if (!is_thread_index_active(i)) continue;
		
		retcode = pthread_kill(threads[i], 0);
		if (retcode == ESRCH) {
			retcode = pthread_join(threads[i], NULL);
			if (retcode) {
				xlog(LOG_ERROR, "xloop: failed to join Thread-%d: %s\n",
				     i, strerror(errno));
			} else {
#ifdef DEBUG
				xlog(LOG_DEBUG, "Thread-%d finished\n", i);
#endif
				/* mark thread as inactive */
				active_threads_bitmask &= (unsigned long long)~(1<<i);
			}
		}
	} 
}


/**
 *
 */
void kill_zombies(pthread_t* threads) 
{
	int i, retcode;
	
	for (i=0; i<cfg->nb_threads; i++) {
		if (!is_thread_index_active(i))
			continue;
		
		retcode = pthread_join(threads[i], NULL);
		if (retcode) {
			xlog(LOG_ERROR, "xloop: failed to join Thread-%d: %s\n",i);
			switch(retcode) {
				case EDEADLK:
					xlog(LOG_ERROR, "%s\n", "Deadlock detected");
					break;
				case EINVAL:
					xlog(LOG_ERROR, "%s\n", "Thread not joinable");
					break;
				case ESRCH:
					xlog(LOG_ERROR, "%s\n", "No thread matches this Id");
					break;
				default :
					xlog(LOG_ERROR, "Unknown errcode %d\n", retcode);
					break;
			}
			
		} else {
			xlog(LOG_DEBUG, "Thread-%d finished\n", i);
		}
	}
}


/**
 *
 */
int proxenet_init_plugins() 
{
	if(proxenet_create_list_plugins(cfg->plugins_path) < 0) {
		xlog(LOG_ERROR, "%s\n", "Failed to build plugins list, leaving");
		return -1;
	}
	
	if(cfg->verbose) {
		xlog(LOG_INFO, "%s\n", "Plugins loaded");
		if (cfg->verbose > 1) {
			xlog(LOG_INFO, "%d plugin(s) found\n",
			     proxenet_plugin_list_size(&plugins_list));
			proxenet_print_plugins_list();
		}
	}
	
	return 0;
}




/**
 *
 */
void xloop(sock_t sock)
{
	fd_set sock_set;
	struct timeval tv;
	int retcode, i;
	pthread_attr_t pattr;
	pthread_t threads[MAX_THREADS];
	pthread_mutex_t threads_mutex[MAX_THREADS]; 
	
	
	xzero(threads, sizeof(pthread_t)*MAX_THREADS);
	
	/* mutex init */
	for (i=0; i<cfg->nb_threads; i++)
		pthread_mutex_init(&threads_mutex[i], NULL);
	
	if (pthread_attr_init(&pattr)) {
		xlog(LOG_ERROR, "%s\n", "Failed to pthread_attr_init");
		return;
	}
	pthread_attr_setdetachstate(&pattr, PTHREAD_CREATE_JOINABLE);
	
	xzero(&tv, sizeof(struct timeval));
	tv.tv_sec = HTTP_TIMEOUT_SOCK;
	tv.tv_usec= 0;
	
	/* proxenet is now running :) */
	proxenet_state = ACTIVE;
	xlog(LOG_INFO, "%s\n", "Starting interactive mode, press h for help");
	
	/* big loop  */
	while (proxenet_state != INACTIVE) {
		sock_t conn;
		retcode = -1;
		
		tv.tv_sec = HTTP_TIMEOUT_SOCK;
		tv.tv_usec= 0;
		
		purge_zombies(threads);
		
		FD_ZERO(&sock_set);
		FD_SET(tty_fd, &sock_set);
		FD_SET(sock, &sock_set);
		
		/* set asynchronous listener */
		retcode = select(sock+1, &sock_set, NULL, NULL, &tv);
		
		if (retcode < 0) {
			xlog(LOG_ERROR, "select: %s\n", strerror(errno));
			proxenet_state = INACTIVE;
			break;
		}
		
		/* event on the listening socket means new request */
		if( FD_ISSET(sock, &sock_set) && proxenet_state!=SLEEPING) {
			struct sockaddr addr;
			socklen_t addrlen = 0;
			int tnum = -1;
			xzero(&addr, sizeof(struct sockaddr));
			
			conn = accept(sock, &addr, &addrlen);    
			if (conn < 0) {
				xlog(LOG_ERROR, "accept failed: %s\n", strerror(errno));
				continue;
			}
			
			/* find next free slot */
			for(tnum=0; active_threads_bitmask & (1<<tnum); tnum++);
			retcode = proxenet_start_new_thread(conn,
							    tnum,
							    &threads[tnum],
							    &threads_mutex[tnum],
							    &pattr);
			if (retcode < 0) {
				xlog(LOG_ERROR, "%s\n", "Error while spawn new thread");
				continue;
			}
			
		} /* end if */
		
		
		/* even on stdin ? -> menu */
		if( FD_ISSET(tty_fd, &sock_set) ) {
			int cmd = tty_getc();
			unsigned int n;
			
			switch (cmd) {
				case 'a':
					n = get_active_threads_size();
					xlog(LOG_INFO, "%ld active thread%c\n", n, (n>1)?'s':' ');
					break;
					
				case 'q':
					xlog(LOG_INFO, "%s\n", "Leaving gracefully");
					proxenet_state = INACTIVE;
					break;
					
				case 'i':
					xlog(LOG_INFO,
					     "Infos:\n"
					     "- Listening interface: %s/%s\n"
					     "- Supported IP version: %s\n"
					     "- Logging to %s\n"
					     "- Running/Max threads: %d/%d\n"
					     "- Plugins dir: %s\n", 
					     cfg->iface,
					     cfg->port,
					     (cfg->ip_version==AF_INET)? "IPv4": (cfg->ip_version==AF_INET6)?"IPv6": "ANY",
					     (cfg->logfile)?cfg->logfile:"stdout",
					     get_active_threads_size(),
					     cfg->nb_threads,
					     cfg->plugins_path			      
					    );
					
					if (proxenet_plugin_list_size())
						proxenet_print_plugins_list();
					else
						xlog(LOG_INFO, "%s\n", "No plugin loaded");
					
					break;
					
				case 'v':
					if (cfg->verbose < MAX_VERBOSE_LEVEL)
					xlog(LOG_INFO, "Verbosity is now %d\n", ++(cfg->verbose));
					break;
					
				case 'b':
					if (cfg->verbose > 0)
						xlog(LOG_INFO, "Verbosity is now %d\n", --(cfg->verbose));
					break;
					
				case 's':
					if (proxenet_state==SLEEPING) {
						xlog(LOG_INFO, "%s\n", "Disabling sleep-mode");
						proxenet_state = ACTIVE;
					} else {
						xlog(LOG_INFO, "%s\n", "Enabling sleep-mode");
						proxenet_state = SLEEPING;
					}
					
					break;
					
				case 'r':
					if (get_active_threads_size()>0) {
						xlog(LOG_ERROR, "%s\n", "Threads still active, cannot reload");
						break;
					}
					
					proxenet_state = SLEEPING; 
					if( proxenet_init_plugins() < 0) {
						if (cfg->verbose)
							xlog(LOG_ERROR, "%s\n", "Failed to reinitilize plugins");
						proxenet_state = INACTIVE;
						break;
					}
					
					proxenet_destroy_plugins_vm();
					proxenet_initialize_plugins_vm();
					
					proxenet_state = ACTIVE;
					
					xlog(LOG_INFO, "%s\n", "Plugins list successfully reloaded");
					if (cfg->verbose) proxenet_print_plugins_list();
					
					break;
					
				case 'h':
					xlog(LOG_INFO, "%s",
					     "Menu:\n"
					     "\ta: print number of active threads\n"
					     "\ts: toggle sleep mode (stop treating requests)\n"
					     "\tr: reload plugins list\n"
					     "\ti: show proxenet info\n"
					     "\tv/b: increase/decrease verbosity\n"
					     "\t[1-9]: disable i-th plugin\n"
					     "\tq: try to quit gently\n"
					     "\th: print this menu\n");
					break;
					
				default:
					if (cmd >= '1' && cmd <= '9') {
						proxenet_switch_plugin(cmd-0x30);
						break;
					}
					
					xlog(LOG_INFO, "Inknown command %#x (h - for help)\n", cmd);
					break;
			}
			
			tty_flush();
		}
		
		
	}  /* endof while(!INACTIVE) */
	
	
	kill_zombies(threads);
	
	proxenet_destroy_plugins_vm();
	
	pthread_attr_destroy(&pattr);
	
	for (i=0; i < cfg->nb_threads; i++)
		pthread_mutex_destroy(&threads_mutex[i]);
	
	return;
}


/**
 *
 * @param signum
 */
void sighandler(int signum)
{
	switch(signum) {
		case SIGINT:
			if (proxenet_state != INACTIVE)
				proxenet_state = INACTIVE;
			break;
			
		default:
			xlog(LOG_WARNING, "No action for signal code %#x\n", signum);
			break;
	}
}


/**
 *
 */
void initialize_sigmask()
{
	struct sigaction saction;
	
	memset(&saction, 0, sizeof(struct sigaction));
	saction.sa_handler = sighandler;
	saction.sa_flags = SA_NOCLDSTOP|SA_NOCLDWAIT;
	sigemptyset(&saction.sa_mask);
	
	sigaction(SIGINT, &saction, NULL);
	sigaction(SIGUSR1, &saction, NULL);
}


/**
 *
 */
int proxenet_start() 
{
	sock_t listening_socket;
	char *err;
	int retcode;
	
	
	/* create listening socket */
	listening_socket = create_bind_socket(cfg->iface, cfg->port, &err);
	if (listening_socket < 0){
		xlog(LOG_CRITICAL, "%s\n", "Cannot create bind socket, leaving");
		return -1;
	}
	
	/* init everything */
	initialize_sigmask();
	plugins_list = NULL;
	proxenet_state = INACTIVE;
	active_threads_bitmask = 0;
	
	/* set up ssl/tls global environnement */
	retcode = proxenet_ssl_init_global_context();
	if (retcode < 0) {
		xlog(LOG_CRITICAL, "%s\n", "Failed to initialize global SSL context");
		return retcode;
	}
	
	/* set up plugins */
	if( proxenet_init_plugins() < 0 ) return -1;
	
	proxenet_initialize_plugins_vm(); // call *MUST* succeed or abort()
	
	
	/* prepare threads and start looping */
	xloop(listening_socket);
	
	
	/* clean context */
	
	proxenet_delete_list_plugins();
	
	proxenet_ssl_free_global_context();
	
	return close_socket(listening_socket, NULL);
}

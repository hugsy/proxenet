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

#ifdef _PYTHON_PLUGIN
#include "plugin-python.h"
#endif

#ifdef _C_PLUGIN
#include "plugin-c.h"
#endif

#ifdef _RUBY_PLUGIN
#include "plugin-ruby.h"
#endif

#ifdef _PERL_PLUGIN
#include "plugin-perl.h"
#endif

#ifdef _LUA_PLUGIN
#include "plugin-lua.h"
#endif


static long	 request_id;
static pthread_mutex_t request_id_mutex;
static pthread_t threads[MAX_THREADS];


/**
 *
 */
int get_new_request_id()
{
	int rid;
	
	pthread_mutex_lock(&request_id_mutex);
	rid = request_id;
	request_id++;
#ifdef DEBUG
	xlog(LOG_DEBUG, "New request #%d\n", rid);
#endif	
	pthread_mutex_unlock(&request_id_mutex);

	return rid;
}


/**
 *
 */
void proxenet_initialize_plugins()
{
	plugin_t *plugin;
	
	for (plugin=plugins_list; plugin; plugin=plugin->next) {
		
		switch (plugin->type) {
			
#ifdef _PYTHON_PLUGIN
			case _PYTHON_:
				if (proxenet_python_initialize_vm(plugin) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "%s\n", "Failed to init Python VM");
					continue;
				}
				
				if (proxenet_python_initialize_function(plugin, REQUEST) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "Failed to init %s in %s\n", CFG_REQUEST_PLUGIN_FUNCTION, plugin->name);
					continue;
				}
				
				if (proxenet_python_initialize_function(plugin, RESPONSE) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "Failed to init %s in %s\n", CFG_RESPONSE_PLUGIN_FUNCTION, plugin->name);
					continue;
				}
				break;
#endif
				
#ifdef _C_PLUGIN
			case _C_:
				if (proxenet_c_initialize_vm(plugin) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "%s\n", "Failed to init C VM");
					continue;
				}
				
				if (proxenet_c_initialize_function(plugin, REQUEST) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "Failed to init %s in %s\n", CFG_REQUEST_PLUGIN_FUNCTION, plugin->name);
					continue;
				}
				
				if (proxenet_c_initialize_function(plugin, RESPONSE) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "Failed to init %s in %s\n", CFG_RESPONSE_PLUGIN_FUNCTION, plugin->name);
					continue;
				}
				break;
#endif

#ifdef _RUBY_PLUGIN
			case _RUBY_:
				if (proxenet_ruby_initialize_vm(plugin) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "%s\n", "Failed to init Ruby VM");
					continue;
				}
				if (proxenet_ruby_initialize_function(plugin, REQUEST) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "Failed to init %s in %s\n", CFG_REQUEST_PLUGIN_FUNCTION, plugin->name);
					continue;
				}
				
				if (proxenet_ruby_initialize_function(plugin, RESPONSE) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "Failed to init %s in %s\n", CFG_RESPONSE_PLUGIN_FUNCTION, plugin->name);
					continue;
				}
				
				break;
#endif

#ifdef _PERL_PLUGIN
			case _PERL_:
				if (proxenet_perl_initialize_vm(plugin) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "%s\n", "Failed to init Perl VM");
					continue;
				}
				break;
#endif				

#ifdef _LUA_PLUGIN
			case _LUA_:
				if (proxenet_lua_initialize_vm(plugin) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "%s\n", "Failed to init Lua VM");
					continue;
				}

				if (proxenet_lua_load_file(plugin) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "Failed to load %s\n", plugin->filename);
					continue;
				}
				
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
			case _PYTHON_: 
				proxenet_python_destroy_vm(p);
				break;
#endif
				
#ifdef _C_PLUGIN
			case _C_:
				proxenet_c_destroy_vm(p);
				break;
#endif

#ifdef _RUBY_PLUGIN
			case _RUBY_:
				proxenet_ruby_destroy_vm(p);
				break;
#endif

#ifdef _PERL_PLUGIN
			case _PERL_:
				proxenet_perl_destroy_vm(p);
				break;
#endif				

#ifdef _LUA_PLUGIN
			case _LUA_:
				proxenet_lua_destroy_vm(p);
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
void proxenet_toggle_plugin(int plugin_id)
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
char* proxenet_apply_plugins(long id, char* data, char type)
{
	plugin_t *p;
	char *new_data, *old_data;
	char* (*plugin_function)(plugin_t*, long, char*, int) = NULL;

	
	old_data = NULL;
	new_data = data;     
	
	for (p=plugins_list; p!=NULL; p=p->next) {  
		
		if (p->state == INACTIVE)
			continue;
		
		switch (p->type) {
			
#ifdef _PYTHON_PLUGIN 
			case _PYTHON_:
				plugin_function = proxenet_python_plugin;
				break;
#endif
				
#ifdef _C_PLUGIN
			case _C_:
				plugin_function = proxenet_c_plugin;
				break;
#endif	  

#ifdef _RUBY_PLUGIN
			case _RUBY_:
				plugin_function = proxenet_ruby_plugin;
				break;
#endif	  

#ifdef _PERL_PLUGIN
			case _PERL_:
				plugin_function = proxenet_perl_plugin;
				break;
#endif	  

#ifdef _LUA_PLUGIN
			case _LUA_:
				plugin_function = proxenet_lua_plugin;
				break;
#endif	  
				
			default:
				xlog(LOG_CRITICAL, "Type %d not supported (yet)\n", p->type);
				continue;
		}

		old_data = new_data;
		new_data = (*plugin_function)(p, id, old_data, type);
		
		if (strcmp(old_data,new_data)) {
			/* if new_data is different, request/response was modified, and  */
			/* another buffer *must* have been allocated, so we can free old one */
			proxenet_xfree(old_data);
		}
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
	int retcode, rid, max_fd;
	fd_set rfds;
	struct timespec ts;
	ssl_context_t ssl_context;
	bool is_ssl;
	sigset_t emptyset;
	
	rid = 0;
	client_socket = retcode =-1;
	proxenet_xzero(&ssl_context, sizeof(ssl_context_t));

	/* wait for any event on sockets */
	for(;;) {
		
		if (server_socket < 0) {
			xlog(LOG_ERROR, "%s\n", "Sock browser->proxy died unexpectedly");
			break;
		}
			
		http_request  = NULL;
		http_response = NULL;
		
		ts.tv_sec  = 5;
		ts.tv_nsec = 0;
		
		max_fd = MAX(client_socket, server_socket) + 1;
		
		FD_ZERO(&rfds);
		FD_SET(server_socket, &rfds);
		FD_SET(client_socket, &rfds);

		sigemptyset(&emptyset);
		retcode = pselect(max_fd, &rfds, NULL, NULL, &ts, &emptyset);
		
		/* if (retcode < 0 && errno == EINTR){ */
			/* continue; */
		/* } */
		if (retcode < 0) {
			xlog(LOG_CRITICAL, "[thread] pselect returned %d: %s\n",
			     retcode, strerror(errno));
			break;
			
		} else if (retcode == 0) {
			break;
		}
		
		is_ssl = ssl_context.use_ssl;
		
		/* is there data from web browser to proxy ? */
		if( FD_ISSET(server_socket, &rfds ) ) {
			
			int n = -1;
			
			if(is_ssl) {
				n = proxenet_read_all(server_socket,
						      &http_request,
						      &(ssl_context.server.context));
			} else {
				n = proxenet_read_all(server_socket, &http_request, NULL);
			}
#ifdef DEBUG
			xlog(LOG_DEBUG, "[%d] Got %dB from client (%s)\n", rid, n, (is_ssl)?"SSL":"PLAIN");
#endif
			
			if (n <= 0) 
				break;
			
			/* is connection to server already established ? */
			if (client_socket < 0) {
				retcode = create_http_socket(http_request, &server_socket, &client_socket, &ssl_context);
				if (retcode < 0) {
					xlog(LOG_ERROR, "[%d] Failed to create proxy->server socket\n", rid);
					proxenet_xfree(http_request);
					break;
				}
				
				if (ssl_context.use_ssl) {
					if (ssl_context.server.is_valid && ssl_context.client.is_valid) {
#ifdef DEBUG
						xlog(LOG_DEBUG, "[%d] SSL interception established\n", rid);
#endif
						proxenet_xfree(http_request);
						continue;
					} else {
						xlog(LOG_ERROR, "[%d] Failed to establish interception\n", rid);
						proxenet_xfree(http_request);
						break;
					}
				}
			}

			/* check if request is valid  */
			if (!is_ssl)
				if (format_http_request(http_request) < 0) {
					proxenet_xfree(http_request);
					break;
				}

			/* got a request, get a request id */
			if (!rid) 
				rid = get_new_request_id();
			
			/* hook request with all plugins in plugins_l  */
			http_request = proxenet_apply_plugins(rid, http_request, REQUEST);

			
#ifdef DEBUG
			xlog(LOG_DEBUG, "[%d] Sending %d bytes (%s)\n",
			     rid, strlen(http_request), (is_ssl) ? "SSL" : "PLAIN");
#endif
			/* send modified data */
			if (is_ssl) {
				retcode = proxenet_ssl_write(client_socket,
							     http_request,
							     strlen(http_request),
							     &(ssl_context.client.context));
			} else {
				retcode = proxenet_write(client_socket, http_request, strlen(http_request));
			}
					
			proxenet_xfree(http_request);

			if (retcode < 0) {
				xlog(LOG_ERROR, "[%d] %s\n", rid, "Failed to write to server");
				break;
			}
			
		} /* end FD_ISSET(data_from_browser) */
		
		
		/* is there data from remote server to proxy ? */
		if( FD_ISSET(client_socket, &rfds ) ) {
			int n = -1;
			
			if (is_ssl)
				n = proxenet_read_all(client_socket, &http_response, &ssl_context.client.context);
			else
				n = proxenet_read_all(client_socket, &http_response, NULL);
			
#ifdef DEBUG
			xlog(LOG_DEBUG, "[%d] Got %dB from server\n", rid, n);
#endif
			
			if (n < 0)
				break;

			
			/* execute response hooks */
			http_response = proxenet_apply_plugins(rid, http_response, RESPONSE);

			
			/* send modified data to client */
			if (is_ssl)
				retcode = proxenet_ssl_write(server_socket, http_response, n, &ssl_context.server.context);
			else
				retcode = proxenet_write(server_socket, http_response, n);
			
			if (retcode < 0) {
				xlog(LOG_ERROR, "[%d] %s\n", rid, "proxy->client: write failed");
			}
			
			proxenet_xfree(http_response);

			/* reset request id for this thread */
			if (rid)
				rid = 0;
			
		}  /*  end FD_ISSET(data_from_server) */
		
	}  /* end for(;;) { select() } */


	/* close client socket */
	if (client_socket > 0) {
		if (ssl_context.client.is_valid) {
			proxenet_ssl_bye(&ssl_context.client.context); 
			proxenet_ssl_free_certificate(&ssl_context.client.cert);
			close_socket_ssl(client_socket, &ssl_context.client.context);
		
		} else {
			close_socket(client_socket);
		}
	}
	
	
	/* close local socket */
	if (server_socket > 0) {
		if (ssl_context.server.is_valid) {
			proxenet_ssl_bye(&ssl_context.server.context);
			proxenet_ssl_free_certificate(&ssl_context.server.cert);
			close_socket_ssl(server_socket, &ssl_context.server.context);
			
		} else {
			close_socket(server_socket);
		}
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
	pthread_t parent_tid;
	
	active_threads_bitmask |= 1 << tinfo->thread_num;
	parent_tid = tinfo->main_tid;
	
	/* treat request */
	proxenet_process_http_request(tinfo->sock, tinfo->plugin_list);

	/* purge thread */
	tinfo->plugin_list = NULL;
	proxenet_xfree(arg);

	/* signal main thread (parent) to clean up */
	if (pthread_kill(parent_tid, SIGCHLD) < 0){
		xlog(LOG_ERROR, "Sending SIGCHLD failed: %s\n", strerror(errno));
	}
	
	pthread_exit(NULL);
}


/**
 *
 */
bool is_thread_active(int idx)
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
		if (is_thread_active(i)) n++;
	
	return n;
}


/**
 *
 */
int proxenet_start_new_thread(sock_t conn, int tnum, pthread_t* thread, pthread_attr_t* tattr)
{
	tinfo_t* tinfo;
	void* tfunc;

	tinfo = (tinfo_t*)proxenet_xmalloc(sizeof(tinfo_t));
	tfunc = &process_thread_job;
	
	tinfo->thread_num = tnum;
	tinfo->sock = conn;
	tinfo->main_tid = pthread_self();
	
	return pthread_create(thread, tattr, tfunc, (void*)tinfo);
}


/**
 *
 */
void purge_zombies()
{
	/* simple threads heartbeat based on pthread_kill response */
	int i, retcode;
	
	for (i=0; i < cfg->nb_threads; i++) {
		if (!is_thread_active(i)) continue;
		
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
void kill_zombies() 
{
	int i, retcode;
	
	for (i=0; i<cfg->nb_threads; i++) {
		if (!is_thread_active(i))
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
int get_new_thread_id()
{
	int tnum;
	
	for(tnum=0; is_thread_active(tnum) && tnum<cfg->nb_threads; tnum++);
	return (tnum >= cfg->nb_threads) ? -1 : tnum;
}


/**
 *
 */
void xloop(sock_t sock)
{
	fd_set sock_set;
	int retcode;
	pthread_attr_t pattr;
	int max_fd, tid;
	sock_t conn;
	sigset_t emptyset;
	
	proxenet_xzero(threads, sizeof(pthread_t) * MAX_THREADS);
	
	if (pthread_attr_init(&pattr)) {
		xlog(LOG_ERROR, "%s\n", "Failed to pthread_attr_init");
		return;
	}
	
	pthread_attr_setdetachstate(&pattr, PTHREAD_CREATE_JOINABLE);

	/* proxenet is now running :) */
	proxenet_state = ACTIVE;
	xlog(LOG_INFO, "%s\n", "Starting interactive mode, press h for help");

	
	/* big loop  */
	while (proxenet_state != INACTIVE) {
		conn = -1;
		retcode = -1;	
		max_fd = MAX(sock, tty_fd) + 1;

		FD_ZERO(&sock_set);
		FD_SET(tty_fd, &sock_set);
		FD_SET(sock, &sock_set);

		purge_zombies();
		
		/* set asynchronous listener */
		sigemptyset(&emptyset);
		retcode = pselect(max_fd, &sock_set, NULL, NULL, NULL, &emptyset);
		
		if (retcode < 0 && errno != EINTR) {
			xlog(LOG_ERROR, "[main] pselect() returned %d: %s\n", retcode, strerror(errno));
			proxenet_state = INACTIVE;
			break;
		}

		if (retcode == 0)
			continue;

		
		/* event on the listening socket -> new request */
		
		if( FD_ISSET(sock, &sock_set) && proxenet_state != SLEEPING) {
			tid = get_new_thread_id();
			if(tid < 0)
				continue;

			struct sockaddr addr;
			socklen_t addrlen = 0;
			proxenet_xzero(&addr, sizeof(struct sockaddr));

			conn = accept(sock, &addr, &addrlen);
			if (conn < 0 && errno != EINTR) {
				xlog(LOG_ERROR, "[main] accept() failed: %s\n", strerror(errno));
				continue;
			}
					
			retcode = proxenet_start_new_thread(conn,tid,&threads[tid],&pattr);
			if (retcode < 0) {
				xlog(LOG_ERROR, "[main] %s\n", "Error while spawn new thread");
				continue;
			}
			
		} /* end if _socket_event */
		
		
		/* even on stdin ? -> menu */
		if( FD_ISSET(tty_fd, &sock_set) ) {
			int cmd = tty_getc();
			unsigned int n = -1;

			if (cmd < 0)
				continue;

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
					     "- SSL private key: %s\n"
					     "- SSL certificate: %s\n"					     
					     "- Plugins directory: %s\n", 
					     cfg->iface,
					     cfg->port,
					     (cfg->ip_version==AF_INET)? "IPv4": (cfg->ip_version==AF_INET6)?"IPv6": "ANY",
					     (cfg->logfile)?cfg->logfile:"stdout",
					     get_active_threads_size(),
					     cfg->nb_threads,					     
					     cfg->keyfile,
					     cfg->certfile,
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
						xlog(LOG_ERROR, "%s\n",
						     "Threads still active, cannot reload");
						break;
					}
					
					proxenet_state = SLEEPING;
					proxenet_delete_list_plugins();
					
					if( proxenet_init_plugins() < 0) {
						if (cfg->verbose)
							xlog(LOG_ERROR, "%s\n",
							     "Failed to reinitilize plugins");
						proxenet_state = INACTIVE;
						break;
					}

					proxenet_destroy_plugins_vm();
					proxenet_initialize_plugins();
					
					proxenet_state = ACTIVE;
					
					xlog(LOG_INFO, "%s\n", "Plugins list successfully reloaded");
					if (cfg->verbose)
						proxenet_print_plugins_list();
					
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
						proxenet_toggle_plugin(cmd-0x30);
						break;
					}
					
					xlog(LOG_INFO, "Unknown command %#x (h - for help)\n", cmd);
					break;
			}
			
			tty_flush();
			
		} /* end if _tty_event*/
		
	}  /* endof while(!INACTIVE) */
	
	
	kill_zombies();
	proxenet_destroy_plugins_vm();
	pthread_attr_destroy(&pattr);
	
	return;
}


/**
 *
 * @param signum
 */
void sighandler(int signum)
{
	switch(signum) {
		case SIGTERM: 
		case SIGINT:
			/* todo faire un compteur de retry sur les quit attempts */
			if (proxenet_state != INACTIVE)
				proxenet_state = INACTIVE;
			break;

		case SIGCHLD:
			purge_zombies();
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
	saction.sa_flags = SA_RESTART|SA_NOCLDSTOP;
	sigemptyset(&saction.sa_mask);
	
	sigaction(SIGINT, &saction, NULL);
	sigaction(SIGTERM, &saction, NULL);
	sigaction(SIGCHLD, &saction, NULL);
}


/**
 *
 */
int proxenet_start() 
{
	sock_t listening_socket;
	char *err;
	
	/* create listening socket */
	listening_socket = create_bind_socket(cfg->iface, cfg->port, &err);
	if (listening_socket < 0) {
		xlog(LOG_CRITICAL, "%s\n", "Cannot create bind socket, leaving");
		return -1;
	}
	
	/* init everything */
	/* initialize_sigmask(); */
	plugins_list = NULL;
	proxenet_state = INACTIVE;
	active_threads_bitmask = 0;
	
	/* set up plugins */
	if( proxenet_init_plugins() < 0 ) return -1;
	
	proxenet_initialize_plugins(); // call *MUST* succeed or abort()

	/* setting request counter  */
	request_id = 0;
	get_new_request_id();
	
	/* prepare threads and start looping */
	xloop(listening_socket);
	
	/* clean context */
	proxenet_delete_list_plugins();
	
	return close_socket(listening_socket);
}

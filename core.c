#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "core.h"
#include "main.h"
#include "utils.h"
#include "socket.h"
#include "plugin.h"
#include "http.h"
#include "ssl.h"
#include "control-server.h"

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
	xlog(LOG_DEBUG, "Allocating ID #%d\n", rid);
#endif
	pthread_mutex_unlock(&request_id_mutex);

	return rid;
}


#ifdef _PERL_PLUGIN
/**
 * Specific initialisation done only once in proxenet whole process.
 *
 * @note Only useful for Perl's plugins right now.
 */
void proxenet_init_once_plugins(int argc, char** argv, char** envp)
{
	proxenet_perl_preinitialisation(argc, argv, envp);
}


/**
 * Specific delete/cleanup done only once in proxenet whole process.
 *
 * @note Only useful for Perl's plugins right now.
 */
void proxenet_delete_once_plugins()
{
	proxenet_perl_postdeletion();
}
#endif


/**
 *
 */
void proxenet_initialize_plugins()
{
	plugin_t *plugin      = plugins_list;
	plugin_t *prec_plugin = NULL;
	plugin_t *next_plugin = NULL;


	while(plugin) {
		
		switch (plugin->type) {
			
#ifdef _PYTHON_PLUGIN
			case _PYTHON_:
				if (proxenet_python_initialize_vm(plugin) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "%s\n", "Failed to init Python VM");
					goto delete_plugin;
				}

				if (proxenet_python_initialize_function(plugin, REQUEST) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "Failed to init %s in %s\n", CFG_REQUEST_PLUGIN_FUNCTION, plugin->name);
					goto delete_plugin;
				}
				
				if (proxenet_python_initialize_function(plugin, RESPONSE) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "Failed to init %s in %s\n", CFG_RESPONSE_PLUGIN_FUNCTION, plugin->name);
					goto delete_plugin;
				}
				break;
#endif
				
#ifdef _C_PLUGIN
			case _C_:
				if (proxenet_c_initialize_vm(plugin) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "%s\n", "Failed to init C VM");
					goto delete_plugin;
				}
				
				if (proxenet_c_initialize_function(plugin, REQUEST) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "Failed to init %s in %s\n", CFG_REQUEST_PLUGIN_FUNCTION, plugin->name);
					goto delete_plugin;
				}
				
				if (proxenet_c_initialize_function(plugin, RESPONSE) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "Failed to init %s in %s\n", CFG_RESPONSE_PLUGIN_FUNCTION, plugin->name);
					goto delete_plugin;
				}
				break;
#endif

#ifdef _RUBY_PLUGIN
			case _RUBY_:
				if (proxenet_ruby_initialize_vm(plugin) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "%s\n", "Failed to init Ruby VM");
					goto delete_plugin;
				}
				if (proxenet_ruby_initialize_function(plugin, REQUEST) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "Failed to init %s in %s\n", CFG_REQUEST_PLUGIN_FUNCTION, plugin->name);
					goto delete_plugin;
				}
				
				if (proxenet_ruby_initialize_function(plugin, RESPONSE) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "Failed to init %s in %s\n", CFG_RESPONSE_PLUGIN_FUNCTION, plugin->name);
					goto delete_plugin;
				}
				
				break;
#endif

#ifdef _PERL_PLUGIN
			case _PERL_:
				if (proxenet_perl_initialize_vm(plugin) < 0) { // TODO voir pour retirer le plugin de la liste, non ?
					plugin->state = INACTIVE;
					plugin->type = -1;
					xlog(LOG_ERROR, "%s\n", "Failed to init Perl VM");
					goto delete_plugin;
				}
				break;
#endif

#ifdef _LUA_PLUGIN
			case _LUA_:
				if (proxenet_lua_initialize_vm(plugin) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "%s\n", "Failed to init Lua VM");
					goto delete_plugin;
				}

				if (proxenet_lua_load_file(plugin) < 0) {
					plugin->state = INACTIVE;
					xlog(LOG_ERROR, "Failed to load %s\n", plugin->filename);
					goto delete_plugin;
				}
				
				break;
#endif
			default:
				break;
		}
		
		prec_plugin = plugin;
		plugin = plugin->next;
		continue;
		
		
delete_plugin:
		if(prec_plugin) {
			prec_plugin->next = plugin->next;
		} else {
			plugins_list = plugin->next;
		}
		
		next_plugin = plugin->next;
		proxenet_free_plugin(plugin);
		plugin = next_plugin;
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
 * @return 0 -> new status inactive 
 * @return 1 -> new status active
 * @return -1 -> not found
 */
int proxenet_toggle_plugin(int plugin_id)
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

			return (plugin->state==INACTIVE ? 0 : 1);
			
		}
	}

	return -1;
}


/**
 *
 */
char* proxenet_apply_plugins(long id, char* data, size_t* data_size, char type)
{
	plugin_t *p;
	char *new_data, *old_data;
	char* (*plugin_function)(plugin_t*, long, char*, size_t*, int) = NULL;

	
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
		new_data = (*plugin_function)(p, id, old_data, data_size, type);
		
		if (new_data!=old_data) {
			/*
			 * If new_data is different, it means a new buffer was allocated by
			 * (*plugin_function)(). The old_data can then be free-ed.
			 */
			proxenet_xfree(old_data);
		} else {
			/* Otherwise, use the old_data, which is only the original data */
			new_data = old_data;
		}
	}
	
	return new_data;
}


/**
 * This function is called by all threads to treat to process the request and response.
 * It will also apply the plugins.
 */
void proxenet_process_http_request(sock_t server_socket)
{
	sock_t client_socket;
	char *http_request, *http_response;
	int retcode, rid;
	size_t length;
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
		
		ts.tv_sec  = HTTP_TIMEOUT_SOCK;
		ts.tv_nsec = 0;
		
		FD_ZERO(&rfds);
		FD_SET(server_socket, &rfds);
		if (client_socket > 0)
			FD_SET(client_socket, &rfds);

		sigemptyset(&emptyset);
		retcode = pselect(FD_SETSIZE, &rfds, NULL, NULL, &ts, &emptyset);
		
		if (retcode < 0) {
			if (errno == EINTR) {
				continue;
			} else {
				xlog(LOG_CRITICAL, "[thread] pselect returned %d: %s\n", retcode, strerror(errno));
				break;
			}
		}

		if (retcode == 0) {
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
			
			/* is connection to server not established ? */
			if (client_socket < 0) {
				retcode = create_http_socket(http_request, &server_socket, &client_socket, &ssl_context);
				if (retcode < 0) {
					xlog(LOG_ERROR, "[%d] Failed to create %s->server socket\n", rid, PROGNAME);
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
			if (!is_ssl && !cfg->proxy.host) {
				if (!is_valid_http_request(http_request)) {
					proxenet_xfree(http_request);
					break;
				}
			}
			
			/* got a request, get a request id */
			if (!rid) 
				rid = get_new_request_id();
			
			/* hook request with all plugins in plugins_list  */
			length = (size_t) n;
			http_request = proxenet_apply_plugins(rid, http_request, &length, REQUEST);
			
#ifdef DEBUG
			xlog(LOG_DEBUG, "[%d] Sending %d bytes (%s)\n", rid, length, (is_ssl)?"SSL":"PLAIN");
#endif
			/* send modified data */
			if (is_ssl) {
				retcode = proxenet_ssl_write(client_socket, http_request, length,
							     &(ssl_context.client.context));
			} else {
				retcode = proxenet_write(client_socket, http_request, length);
			}
			
			proxenet_xfree(http_request);

			if (retcode < 0) {
				xlog(LOG_ERROR, "[%d] %s\n", rid, "Failed to write to server");
				if (rid)
					rid = 0;
				break;
			}
			
		} /* end FD_ISSET(data_from_browser) */
		
		
		/* is there data from remote server to proxy ? */
		if( client_socket > 0 && FD_ISSET(client_socket, &rfds ) ) {
			int n = -1;
			
			if (is_ssl)
				n = proxenet_read_all(client_socket, &http_response, &ssl_context.client.context);
			else
				n = proxenet_read_all(client_socket, &http_response, NULL);
			
#ifdef DEBUG
			xlog(LOG_DEBUG, "[%d] Got %dB from server\n", rid, n);
#endif
			
			if (n <= 0)
				break;

			
			/* execute response hooks */
			length = (size_t) n;
			http_response = proxenet_apply_plugins(rid, http_response, &length, RESPONSE);

			/* send modified data to client */
			if (is_ssl)
				retcode = proxenet_ssl_write(server_socket, http_response, length, &ssl_context.server.context);
			else
				retcode = proxenet_write(server_socket, http_response, length);
			
			if (retcode < 0) {
				xlog(LOG_ERROR, "[%d] %s\n", rid, "proxy->client: write failed");
			}
			
			proxenet_xfree(http_response);

			/* reset request id for this thread */
			if (rid)
				rid = 0;
			
		}  /* end FD_ISSET(data_from_server) */
		
	}  /* end for(;;) { select() } */


	/* close client socket */
	if (client_socket > 0) {
		if (ssl_context.client.is_valid) {
			proxenet_ssl_finish(&ssl_context.client);
			close_socket_ssl(client_socket, &ssl_context.client.context);
		
		} else {
			close_socket(client_socket);
		}
	}
	
	
	/* close local socket */
	if (server_socket > 0) {
		if (ssl_context.server.is_valid) {
			proxenet_ssl_finish(&(ssl_context.server));
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
	proxenet_process_http_request(tinfo->sock);

	/* purge thread */
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
int proxenet_initialize_plugins_list()
{
	if(proxenet_create_list_plugins(cfg->plugins_path) < 0) {
		xlog(LOG_ERROR, "%s\n", "Failed to build plugins list, leaving");
		return -1;
	}
	
	if(cfg->verbose) {
		xlog(LOG_INFO, "%s\n", "Plugins loaded");
		if (cfg->verbose > 1) {
			xlog(LOG_INFO, "%d plugin(s) found\n", proxenet_plugin_list_size());
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
void xloop(sock_t sock, sock_t ctl_sock)
{
	fd_set sock_set;
	int retcode;
	pthread_attr_t pattr;
	int tid;
	sock_t conn;
	sigset_t curmask, oldmask;
	sock_t ctl_cli_sock = -1;

	/* prepare threads  */
	proxenet_xzero(threads, sizeof(pthread_t) * MAX_THREADS);
	
	if (pthread_attr_init(&pattr)) {
		xlog(LOG_ERROR, "%s\n", "Failed to pthread_attr_init");
		return;
	}
	
	pthread_attr_setdetachstate(&pattr, PTHREAD_CREATE_JOINABLE);

	/* block useful signal */
	sigemptyset(&curmask);
	sigaddset(&curmask, SIGTERM);
	sigaddset(&curmask, SIGINT);
	sigaddset(&curmask, SIGCHLD);
	if (pthread_sigmask(SIG_BLOCK, &curmask, &oldmask) < 0) {
		xlog(LOG_ERROR, "sigprocmask failed : %s\n", strerror(errno));
		return;
	}

	/* proxenet is now running :) */
	proxenet_state = ACTIVE;
	
	/* big loop  */
	while (proxenet_state != INACTIVE) {
		conn = -1;
		retcode = -1;	
		
		FD_ZERO(&sock_set);

		FD_SET(sock, &sock_set);
		FD_SET(ctl_sock, &sock_set);
		if (ctl_cli_sock > 0)
			FD_SET(ctl_cli_sock, &sock_set);

		purge_zombies();
		
		/* set asynchronous listener */
		struct timespec timeout = {
			.tv_sec = 5,
			.tv_nsec = 0
		};
		
		retcode = pselect(FD_SETSIZE, &sock_set, NULL, NULL, &timeout, &oldmask);
		
		if (retcode < 0) {
			if (errno != EINTR) {
				xlog(LOG_ERROR, "[main] pselect() returned %d: %s\n", retcode, strerror(errno));
				proxenet_state = INACTIVE;
				break;
			} else {
				continue;
			}
		}
		
		if (retcode == 0)
			continue;

		if (proxenet_state == INACTIVE)
			break;
		
		/* event on the listening socket -> new request */
		if( FD_ISSET(sock, &sock_set) && proxenet_state != SLEEPING) {
#ifdef DEBUG
			xlog(LOG_DEBUG, "%s\n", "Incoming listening event");
#endif

			tid = get_new_thread_id();
			if(tid < 0)
				continue;

			struct sockaddr addr;
			socklen_t addrlen = 0;

			proxenet_xzero(&addr, sizeof(struct sockaddr));

			conn = accept(sock, &addr, &addrlen); 
			if (conn < 0) {
				if(errno != EINTR)
					xlog(LOG_ERROR, "[main] accept() failed: %s\n", strerror(errno));
				continue;
			}
			
			retcode = proxenet_start_new_thread(conn, tid, &threads[tid], &pattr);
			if (retcode < 0) {
				xlog(LOG_ERROR, "[main] %s\n", "Error while spawn new thread");
				continue;
			}
			
		} /* end if _socket_event */
		
		
		/* event on control listening socket */
		if( FD_ISSET(ctl_sock, &sock_set) ) {
#ifdef DEBUG
			xlog(LOG_DEBUG, "%s\n", "Incoming control event");
#endif
			struct sockaddr_un sun_cli;
			socklen_t sun_cli_len = 0;
			int new_conn = -1;
			
			proxenet_xzero(&sun_cli, sizeof(struct sockaddr_un));

			new_conn = accept(ctl_sock, (struct sockaddr *)&sun_cli, &sun_cli_len);
			if (new_conn < 0) {
				xlog(LOG_ERROR, "[main] control accept() failed: %s\n", strerror(errno));
				continue;
			}
			
			if (ctl_cli_sock < 0) {
				ctl_cli_sock = new_conn;
				xlog(LOG_INFO, "%s\n", "New connection on Control socket");
				proxenet_write(ctl_cli_sock, CONTROL_MOTD, strlen(CONTROL_MOTD));
				proxenet_write(ctl_cli_sock, CONTROL_PROMPT, strlen(CONTROL_PROMPT));
				
			} else {
				if(new_conn > 0) {
					xlog(LOG_ERROR, "%s\n", "Denied control connection: already established");
					if(close_socket(new_conn) < 0) {
						xlog(LOG_ERROR, "Failed to close socket: %s\n", strerror(errno));
					}
				}
			}
			
		}/* end if _control_listening_event */

		
		/* event on control socket */
		if( ctl_cli_sock > 0 && FD_ISSET(ctl_cli_sock, &sock_set) ) {
			
			if (proxenet_handle_control_event(&ctl_cli_sock) < 0) {
				close_socket(ctl_cli_sock);
				ctl_cli_sock = -1;
			}
			
		} /* end if _control_event */
		
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
#ifdef DEBUG
	xlog(LOG_DEBUG, "Received signal %s [%d]\n", strsignal(signum), signum);
#endif
	
	switch(signum) {
		
		case SIGTERM: 
		case SIGINT:
			if (proxenet_state != INACTIVE) 
				proxenet_state = INACTIVE;
			
			cfg->try_exit++;
			xlog(LOG_INFO, "%s, %d/%d\n", "Trying to leave", cfg->try_exit, cfg->try_exit_max);
			
			if (cfg->try_exit == cfg->try_exit_max) {
				xlog(LOG_CRITICAL, "%s\n", "Failed to exit properly");
				abort();
			}
			
			break;

		case SIGCHLD:
			purge_zombies();
			break;
	}
}


/**
 *
 */
void initialize_sigmask(struct sigaction *saction)
{
	memset(saction, 0, sizeof(struct sigaction));
	
	saction->sa_handler = sighandler;
	saction->sa_flags = SA_RESTART|SA_NOCLDSTOP;
	sigemptyset(&(saction->sa_mask));
	
	sigaction(SIGINT,  saction, NULL);
	sigaction(SIGTERM, saction, NULL);
	sigaction(SIGCHLD, saction, NULL);

	saction->sa_handler = SIG_IGN;
	sigaction(SIGPIPE,  saction, NULL);
}


/**
 *
 */
int proxenet_start() 
{
	sock_t control_socket, listening_socket;
	struct sigaction saction;
	
	control_socket = listening_socket = -1;

	/* create control socket */
	control_socket = create_control_socket();
	if (control_socket < 0) {
		xlog(LOG_CRITICAL, "Cannot create control socket: %s\n", strerror(errno));
		return -1;
	}

#ifdef DEBUG
	xlog(LOG_INFO, "Control socket: %d\n", control_socket);
#endif
	
	/* create listening socket */
	listening_socket = create_bind_socket(cfg->iface, cfg->port);
	if (listening_socket < 0) {
		xlog(LOG_CRITICAL, "Cannot create bind socket: %s\n", strerror(errno));
		return -1;
	}

#ifdef DEBUG
	xlog(LOG_INFO, "Bind socket: %d\n", listening_socket);
#endif
	
	/* init everything */
	initialize_sigmask(&saction);

	plugins_list = NULL;
	proxenet_state = INACTIVE;
	active_threads_bitmask = 0;
	
	/* set up plugins */
	if( proxenet_initialize_plugins_list() < 0 )
		return -1;
	
	proxenet_initialize_plugins(); // call *MUST* succeed or abort()

	/* setting request counter  */
	request_id = 0;
	get_new_request_id();
	
	/* prepare threads and start looping */
	xloop(listening_socket, control_socket);
	
	/* clean context */
	proxenet_delete_list_plugins();
	
	close_socket(listening_socket);
	close_socket(control_socket);

	unlink(CONTROL_SOCK_PATH);
	return 0;
}

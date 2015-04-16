#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _PYTHON_PLUGIN
/* Python2 API specificity : http://docs.python.org/2/c-api/intro.html#includes */
#include <Python.h>
#endif

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
#include <fnmatch.h>
#include <time.h>

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

#ifdef _TCL_PLUGIN
#include "plugin-tcl.h"
#endif

#ifdef _JAVA_PLUGIN
#include "plugin-java.h"
#endif


static pthread_mutex_t request_id_mutex;


/**
 * Allocate safely a new and unique request id
 *
 * @return the new request id
 */
static unsigned long get_new_request_id()
{
        unsigned long rid;

        pthread_mutex_lock(&request_id_mutex);
        rid = request_id;
        request_id++;
        pthread_mutex_unlock(&request_id_mutex);
#ifdef DEBUG
        xlog(LOG_DEBUG, "Allocating ID #%ld\n", rid);
#endif
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
                                if (proxenet_perl_initialize_vm(plugin) < 0) {
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

#ifdef _TCL_PLUGIN
                        case _TCL_:
                                if (proxenet_tcl_initialize_vm(plugin) < 0) {
                                        plugin->state = INACTIVE;
                                        xlog(LOG_ERROR, "%s\n", "Failed to init Lua VM");
                                        goto delete_plugin;
                                }

                                if (proxenet_tcl_load_file(plugin) < 0) {
                                        plugin->state = INACTIVE;
                                        xlog(LOG_ERROR, "Failed to load %s\n", plugin->filename);
                                        goto delete_plugin;
                                }

                                break;
#endif

#ifdef _JAVA_PLUGIN
                        case _JAVA_:
                                if (proxenet_java_initialize_vm(plugin) < 0) {
                                        plugin->state = INACTIVE;
                                        xlog(LOG_ERROR, "%s\n", "Failed to init Java VM");
                                        goto delete_plugin;
                                }

                                if (proxenet_java_load_file(plugin) < 0) {
                                        plugin->state = INACTIVE;
                                        xlog(LOG_ERROR, "Failed to load %s\n", plugin->filename);
                                        goto delete_plugin;
                                }

                                break;
#endif
                        default:
                                break;
                }

                if (cfg->verbose > 1)
                        xlog(LOG_INFO, "Successfully initialized '%s'\n", plugin->filename);

                prec_plugin = plugin;
                plugin = plugin->next;
                continue;


        delete_plugin:
                if(prec_plugin) {
                        prec_plugin->next = plugin->next;
                } else {
                        plugins_list = plugin->next;
                }

                if (cfg->verbose > 1)
                        xlog(LOG_ERROR, "Removing '%s' from plugin list\n", plugin->filename);

                next_plugin = plugin->next;
                proxenet_free_plugin(plugin);
                plugin = next_plugin;
        }

}


/**
 * Disable plugins and kill all the interpreters allocated in a clean (i.e. API-provided) way.
 *
 * Caller must make sure proxenet is in safe state (child threads are joined) before calling
 * this function.
 */
void proxenet_destroy_plugins_vm()
{
        plugin_t *p;
        interpreter_t *vm;

        for (p=plugins_list; p!=NULL; p=p->next) {

                vm = NULL;

                if (!p->interpreter->ready)
                        continue;

                /*
                 * The function `proxenet_*_destroy_plugin()` must set the current plugin as
                 * INACTIVE, remove all the active references and clear allocated blocks.
                 */
                switch (p->type) {
#ifdef _PYTHON_PLUGIN
                        case _PYTHON_:   proxenet_python_destroy_plugin(p); break;
#endif
#ifdef _C_PLUGIN
                        case _C_:        proxenet_c_destroy_plugin(p); break;
#endif
#ifdef _RUBY_PLUGIN
                        case _RUBY_:     proxenet_ruby_destroy_plugin(p); break;
#endif
#ifdef _PERL_PLUGIN
                        case _PERL_:     proxenet_perl_destroy_plugin(p); break;
#endif
#ifdef _LUA_PLUGIN
                        case _LUA_:      proxenet_lua_destroy_plugin(p); break;
#endif
#ifdef _TCL_PLUGIN
                        case _TCL_:      proxenet_tcl_destroy_plugin(p); break;
#endif
#ifdef _JAVA_PLUGIN
                        case _JAVA_:     proxenet_java_destroy_plugin(p); break;
#endif
                        default: break;
                }
                if(cfg->verbose)
                        xlog(LOG_INFO, "Plugin '%s' has been destroyed.\n", p->name);

                if (count_initialized_plugins_by_type(p->type) > 0)
                        continue;

                vm = p->interpreter;

                /*
                 * The function `proxenet_*_destroy_vm()` calls API to delete the VM. The function
                 * is called when the last plugin[type] has been destroyed
                 */
                switch (p->type) {
#ifdef _PYTHON_PLUGIN
                        case _PYTHON_:   proxenet_python_destroy_vm(vm); break;
#endif
#ifdef _C_PLUGIN
                        case _C_:        proxenet_c_destroy_vm(vm); break;
#endif
#ifdef _RUBY_PLUGIN
                        case _RUBY_:     proxenet_ruby_destroy_vm(vm); break;
#endif
#ifdef _PERL_PLUGIN
                        case _PERL_:     proxenet_perl_destroy_vm(vm); break;
#endif
#ifdef _LUA_PLUGIN
                        case _LUA_:      proxenet_lua_destroy_vm(vm); break;
#endif
#ifdef _TCL_PLUGIN
                        case _TCL_:      proxenet_tcl_destroy_vm(vm); break;
#endif
#ifdef _JAVA_PLUGIN
                        case _JAVA_:     proxenet_java_destroy_vm(vm); break;
#endif
                        default: break;
                }

                if(cfg->verbose)
                        xlog(LOG_INFO, "VM %s has been destroyed.\n", supported_plugins_str[p->type]);
        }

        return;
}


/**
 * (De)Activate a plugin at runtime
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
                                case INACTIVE: plugin->state = ACTIVE;   break;
                                case ACTIVE:   plugin->state = INACTIVE; break;
                                default: break;
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
static int proxenet_apply_plugins(request_t *request)
{
        plugin_t *p = NULL;
        char *old_data = NULL;;
        char* (*plugin_function)(plugin_t*, request_t*) = NULL;

        for (p=plugins_list; p!=NULL; p=p->next) {

                if (p->state == INACTIVE)
                        continue;

                switch (p->type) {

#ifdef _PYTHON_PLUGIN
                        case _PYTHON_:  plugin_function = proxenet_python_plugin; break;
#endif
#ifdef _C_PLUGIN
                        case _C_:       plugin_function = proxenet_c_plugin; break;
#endif
#ifdef _RUBY_PLUGIN
                        case _RUBY_:    plugin_function = proxenet_ruby_plugin; break;
#endif
#ifdef _PERL_PLUGIN
                        case _PERL_:    plugin_function = proxenet_perl_plugin; break;
#endif
#ifdef _LUA_PLUGIN
                        case _LUA_:     plugin_function = proxenet_lua_plugin; break;
#endif
#ifdef _TCL_PLUGIN
                        case _TCL_:     plugin_function = proxenet_tcl_plugin; break;
#endif
#ifdef _JAVA_PLUGIN
                        case _JAVA_:    plugin_function = proxenet_java_plugin; break;
#endif
                        default:
                                xlog(LOG_CRITICAL, "Type %d not supported (yet)\n", p->type);
                                return -1;
                }

#ifdef DEBUG
                xlog(LOG_DEBUG,
                     "Calling '%s:%s' with rid=%d (%s)\n",
                     p->name,
                     request->type==REQUEST?CFG_REQUEST_PLUGIN_FUNCTION:CFG_RESPONSE_PLUGIN_FUNCTION,
                     request->id,
                     supported_plugins_str[p->type]
                     );
#endif

                old_data = request->data;
                request->data = (*plugin_function)(p, request);

                if (request->data) {
                        /*
                         * If new_data is different, it means a new buffer was allocated by
                         * (*plugin_function)(). The old_data can then be free-ed.
                         */
                        proxenet_xfree(old_data);

#ifdef DEBUG
                        xlog(LOG_DEBUG,
                             "New data from '%s:%s' on rid=%d\n",
                             request->type==REQUEST?CFG_REQUEST_PLUGIN_FUNCTION:CFG_RESPONSE_PLUGIN_FUNCTION,
                             p->name,
                             p->id);
#endif

                } else {
                        /* Otherwise (data different or error), use the original data */
                        request->data = old_data;

#ifdef DEBUG
                        xlog(LOG_DEBUG,
                             "No new data from '%s:%s' on rid=%d\n",
                             request->type==REQUEST?CFG_REQUEST_PLUGIN_FUNCTION:CFG_RESPONSE_PLUGIN_FUNCTION,
                             p->name,
                             p->id);
#endif
                }

                /* Additionnal check for dummy plugin coder */
                if (!request->data || !request->size) {
                        xlog(LOG_CRITICAL, "Plugin '%s' is invalid, disabling\n", p->name);
                        p->state = INACTIVE;

                        if (cfg->verbose){
                                if(!request->data)
                                        xlog(LOG_CRITICAL, "Plugin '%s' returned a NULL value\n", p->name);
                                else
                                        xlog(LOG_CRITICAL, "Plugin '%s' returned a NULL size\n", p->name);
                        }

                        return -1;
                }
        }

        return 0;
}


/**
 * This function is called by all threads to treat to process the request and response.
 * It will also apply the plugins.
 *
 * @param server_socket is the socket received by the main thread from the web browser (acting like server
 * to web browser)
 */
void proxenet_process_http_request(sock_t server_socket)
{
        sock_t client_socket;
        request_t req;
        int retcode, n;
        fd_set rfds;
        struct timespec ts;
        ssl_context_t ssl_context;
        bool is_ssl;
        sigset_t emptyset;
        bool is_new_http_connection = false;

        client_socket = retcode = n = -1;
        proxenet_xzero(&req, sizeof(request_t));
        proxenet_xzero(&ssl_context, sizeof(ssl_context_t));

        /* wait for any event on sockets */
        while(proxy_state == ACTIVE) {

                if (server_socket < 0) {
                        xlog(LOG_ERROR, "sock browser->%s (#%d) died unexpectedly\n", PROGNAME, server_socket);
                        break;
                }

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
                        continue;
                }

                is_ssl = ssl_context.use_ssl;

                /* is there data from web browser to proxy ? */
                if( FD_ISSET(server_socket, &rfds ) ) {

                        if(is_ssl && req.do_intercept) {
                                n = proxenet_read_all(server_socket,
                                                      &req.data,
                                                      &(ssl_context.server.context));
                        } else {
                                n = proxenet_read_all(server_socket, &req.data, NULL);
                        }

#ifdef DEBUG
                        xlog(LOG_DEBUG, "Got %dB from client (%s srv_sock=#%d intercept_flag=%s)\n",
                             n, is_ssl?"SSL":"PLAIN", server_socket,
                             req.do_intercept?"true":"false");
#endif

                        if (n < 0) {
                                xlog(LOG_ERROR, "%s\n", "read() failed, end thread");
                                break;
                        }

                        if (n == 0){
#ifdef DEBUG
                                xlog(LOG_DEBUG, "%s\n", "Socket EOF from client");
#endif
                                break;
                        }


                        /* from here, n can only be positive */
                        req.size = (size_t) n;
                        bytes_sent += n;

                        if (req.id > 0 && !req.do_intercept){
#ifdef DEBUG
                                xlog(LOG_DEBUG, "Intercept disabled for browser->'%s'\n", req.http_infos.hostname);
#endif
                                goto send_to_server;
                        }

                        /* proxy keep-alive */
                        if (req.id > 0){
                                request_t* old_req = (request_t*)proxenet_xmalloc(sizeof(request_t));
                                memcpy(old_req, &req, sizeof(request_t));
                                char* host = proxenet_xstrdup2( req.http_infos.hostname );

                                free_http_infos(&(req.http_infos));
                                update_http_infos(&req);

                                if (strcmp( host, req.http_infos.hostname )){
                                        /* reset the client connection parameters */
                                        if (cfg->verbose)
                                                xlog(LOG_INFO, "Reusing sock=%d (old request=%d, old sock=%d) %s/%s\n",
                                                     server_socket, req.id, client_socket, host, req.http_infos.hostname );
                                        proxenet_close_socket(client_socket, &(ssl_context.client));
                                        free_http_infos(&(req.http_infos));
                                        client_socket = -1;
                                }

                                proxenet_xclean( old_req, sizeof(request_t) );
                                proxenet_xfree( host );
                        }

                        req.type = REQUEST;
                        req.id = get_new_request_id();

                        /* is connection to server not established ? -> new request */
                        if ( client_socket < 0) {
                                retcode = create_http_socket(&req, &server_socket, &client_socket, &ssl_context);
                                if (retcode < 0) {
                                        xlog(LOG_ERROR, "Failed to create %s->server socket\n", PROGNAME);
                                        proxenet_xfree(req.data);
                                        break;
                                }


                                if (ssl_context.use_ssl) {
                                        req.is_ssl = true;

                                        if (req.do_intercept == false) {
#ifdef DEBUG
                                                xlog(LOG_DEBUG, "SSL interception client <-> %s <-> server disabled\n", PROGNAME);
#endif
                                                proxenet_xfree(req.data);
                                                req.type = REQUEST;
                                                req.id = get_new_request_id();
                                                continue;

                                        } else if (ssl_context.server.is_valid && ssl_context.client.is_valid) {
#ifdef DEBUG
                                                xlog(LOG_DEBUG, "SSL interception client <-> %s <-> server established\n", PROGNAME);
#endif
                                                proxenet_xfree(req.data);
                                                is_new_http_connection = true;
                                                continue;
                                        }

                                        xlog(LOG_ERROR, "%s\n", "Failed to establish interception");
                                        proxenet_xfree(req.data);
                                        break;
                                }

                                is_new_http_connection = true;
                        }



                        /* if proxenet does not relay to another proxy */
                        if (!cfg->proxy.host) {

                                if (is_new_http_connection) {

                                        if (is_ssl) {
                                                /*
                                                 * SSL request fields still have the values gathered in the CONNECT
                                                 * Those values must be updated to reflect the real request
                                                 */
                                                free_http_infos(&(req.http_infos));
                                                retcode = update_http_infos(&req);
                                        } else {
                                                /*
                                                 * Some browsers send plain HTTP requests like this
                                                 * GET http://foo/bar.blah HTTP/1.1 ...
                                                 * Format those kinds of requests stripping out proto & hostname
                                                 */
                                                retcode = format_http_request(&req.data, &req.size);
                                        }
                                } else {
                                        /* if here, at least 1 request has been to server */
                                        /* so simply forward  */
                                        /* e.g. using HTTP/1.1 100 Continue */
#ifdef DEBUG
                                        xlog(LOG_DEBUG, "Resuming stream '%d'->'%d'\n", client_socket, server_socket);
#endif
                                        free_http_infos(&(req.http_infos));
                                        retcode = update_http_infos(&req);
                                }

                                if (retcode < 0){
                                        xlog(LOG_ERROR, "Failed to update %s information in request %d\n",
                                             is_ssl?"HTTPS":"HTTP", req.id);
                                        proxenet_xfree(req.data);
                                        break;
                                }
                        }


                        if (cfg->verbose) {
                                xlog(LOG_INFO, "%s request to '%s:%d'\n",
                                     is_ssl?"SSL":"plain", req.http_infos.hostname, req.http_infos.port);

                                if (cfg->verbose > 1)
                                        xlog(LOG_INFO, "%s %s %s\n",
                                             req.http_infos.method, req.http_infos.uri, req.http_infos.version);
                        }

#ifdef DEBUG
                        xlog(LOG_DEBUG, "Request %d pre-plugins: buflen:%lu\n",
                             req.id, req.size);
#endif
                        /* hook request with all plugins in plugins_list  */
                        if ( proxenet_apply_plugins(&req) < 0) {
                                /* extremist action: any error on any plugin discard the whole request */
                                proxenet_xfree( req.data );
                                break;
                        }
#ifdef DEBUG
                        xlog(LOG_DEBUG, "Request %d post-plugins: buflen:%lu\n",
                             req.id, req.size);

                        if(cfg->verbose > 2)
                                proxenet_hexdump(req.data, req.size);
#endif

                send_to_server:
                        /* send modified data */
                        if (is_ssl && req.do_intercept) {
                                retcode = proxenet_ssl_write(&(ssl_context.client.context), req.data, req.size);
                        } else {
                                retcode = proxenet_write(client_socket, req.data, req.size);
                        }

                        /* reset data */
                        proxenet_xfree(req.data);
                        req.size = 0;

                        if (retcode < 0) {
                                xlog(LOG_ERROR, "[%d] %s\n", req.id, "Failed to write to server");
                                break;
                        }

#ifdef DEBUG
                        xlog(LOG_DEBUG, "Written %d bytes to server (socket=%s socket #%d)\n",
                             retcode, is_ssl?"SSL":"PLAIN", client_socket);
#endif

                } /* end FD_ISSET(data_from_browser) */


                /* is there data from remote server to proxy ? */
                if( client_socket > 0 && FD_ISSET(client_socket, &rfds ) ) {

                        if(req.is_ssl && req.do_intercept) {
                                n = proxenet_read_all(client_socket, &req.data, &ssl_context.client.context);
                        } else {
                                n = proxenet_read_all(client_socket, &req.data, NULL);
                        }

                        if (n < 0){
                                xlog(LOG_ERROR, "read() %s on cli_sock=#%d failed: %d\n",
                                     is_ssl?"SSL":"PLAIN",
                                     client_socket, n);
                                break;
                        }

                        if (n==0){
#ifdef DEBUG
                                xlog(LOG_DEBUG, "Socket EOF from server (cli_sock=#%d)\n",
                                     client_socket);
#endif
                                break;
                        }

                        /* update request data structure */
                        req.type   = RESPONSE;

                        /* from here, n can only be positive */
                        req.size   = (size_t) n;
                        bytes_recv += n;

                        if (req.do_intercept==false){
#ifdef DEBUG
                                xlog(LOG_DEBUG, "Intercept disabled for '%s'->browser\n", req.http_infos.hostname);
#endif
                                goto send_to_client;
                        }

#ifdef DEBUG
                        xlog(LOG_DEBUG, "Response %d pre-plugins: buflen:%lu\n",
                             req.id, req.size);
#endif
                        /* execute response hooks */
                        if ( proxenet_apply_plugins(&req) < 0) {
                                /* extremist action: any error on any plugin discard the whole request */
                                proxenet_xfree(req.data);
                                break;
                        }
#ifdef DEBUG
                        xlog(LOG_DEBUG, "Response %d post-plugins: buflen:%lu\n",
                             req.id, req.size);

                        if(cfg->verbose > 2)
                                proxenet_hexdump(req.data, req.size);
#endif

                send_to_client:
                        /* send modified data to client */
                        if (req.is_ssl && req.do_intercept)
                                retcode = proxenet_ssl_write(&(ssl_context.server.context), req.data, req.size);
                        else
                                retcode = proxenet_write(server_socket, req.data, req.size);

                        if (retcode < 0) {
                                xlog(LOG_ERROR, "[%d] %s\n", req.id, "proxy->client: write failed");
                                proxenet_xfree(req.data);
                                break;
                        }

#ifdef DEBUG
                        xlog(LOG_DEBUG, "Written %d bytes to browser (socket=%s socket #%d)\n",
                             retcode, is_ssl?"SSL":"PLAIN", client_socket);
#endif
                        proxenet_xfree(req.data);

                }  /* end FD_ISSET(data_from_server) */

        }  /* end for(;;) { select() } */


        if (req.id) {
#ifdef DEBUG
                xlog(LOG_DEBUG, "Free-ing request %d\n", req.id);
#endif
                free_http_infos(&(req.http_infos));
        }


        /* close client socket */
        if (client_socket > 0) {
#ifdef DEBUG
                xlog(LOG_DEBUG, "Closing %s->server socket #%d\n", PROGNAME, client_socket);
#endif

                proxenet_close_socket(client_socket, &(ssl_context.client));
        }


        /* close local socket */
        if (server_socket > 0) {
#ifdef DEBUG
                xlog(LOG_DEBUG, "Closing browser->%s socket #%d\n", PROGNAME, server_socket);
#endif
                proxenet_close_socket(server_socket, &(ssl_context.server));
        }


#ifdef DEBUG
        xlog(LOG_DEBUG, "%s\n", "Structures closed, leaving");
#endif
        /* and that's all folks */
        return;
}


/**
 *
 */
static void mark_thread_as_active(int i)
{
        active_threads_bitmask |= (1 << i);
        return;
}


/**
 *
 */
static void mark_thread_as_inactive(int i)
{
        active_threads_bitmask &= (unsigned long long)~(1 << i);
        return;
}


/**
 * Checks active threads bitmask to determine if a thread is alive.
 *
 * @param idx is the index of the thread to look up for
 * @return true if this thread still alive
 */
bool is_thread_active(int idx)
{
        return active_threads_bitmask & (1<<idx);
}


/**
 *
 */
static void* process_thread_job(void* arg)
{
        tinfo_t* tinfo = (tinfo_t*) arg;
        pthread_t parent_tid;
#ifdef DEBUG
        unsigned int tnum = tinfo->thread_num;
        xlog(LOG_DEBUG, "Starting thread %d\n", tnum);
#endif

        parent_tid = tinfo->main_tid;

        /* treat request */
        proxenet_process_http_request(tinfo->sock);

        /* purge thread */
        proxenet_xfree(arg);

        /* signal main thread (parent) to clean up */
        if (pthread_kill(parent_tid, SIGCHLD) < 0){
                xlog(LOG_ERROR, "Sending SIGCHLD failed: %s\n", strerror(errno));
        }

#ifdef DEBUG
        xlog(LOG_DEBUG, "Ending thread %d\n", tnum);
#endif

        pthread_exit(0);
}


/**
 * Returns the number of threads alive.
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
static int proxenet_start_new_thread(sock_t conn, int tnum, pthread_t* thread, pthread_attr_t* tattr)
{
        tinfo_t* tinfo;
        void* tfunc;

        tinfo = (tinfo_t*)proxenet_xmalloc(sizeof(tinfo_t));
        tfunc = &process_thread_job;

        tinfo->thread_num = tnum;
        tinfo->sock = conn;
        tinfo->main_tid = pthread_self();

        mark_thread_as_active( tnum );

        return pthread_create(thread, tattr, tfunc, (void*)tinfo);
}


/**
 *
 */
static void proxenet_join_thread(int i)
{
        int ret;

        ret = pthread_join(threads[i], NULL);
        switch(ret){
                case EDEADLK:
                case EINVAL:
                        xlog(LOG_ERROR, "proxenet_join_thread() joining thread-%d produced error %d\n", i, ret);
                        break;

                case ESRCH:
                        xlog(LOG_WARNING, "proxenet_join_thread() could not find thread-%d\n", i);

                default:
#ifdef DEBUG
                        xlog(LOG_DEBUG, "Thread-%d has finished\n", i);
#endif
                        mark_thread_as_inactive( i );
        }

        return;
}


/**
 * This function is a simple heartbeat for proxenet threads (childs only). It is
 * based on pthread_kill(), probing for ESRCH return value to check if a thread has
 * ended and clean the bitmask accordingly.
 */
static void purge_zombies()
{
        int i;

        for (i=0; i < cfg->nb_threads; i++) {
                if (!is_thread_active(i)) continue;

                if (pthread_kill(threads[i], 0) == ESRCH) {
#ifdef DEBUG
                        xlog(LOG_DEBUG, "Joining thread-%d\n", i);
#endif
                        proxenet_join_thread(i);
                }
        }

        return;
}


/**
 *
 */
static void kill_zombies()
{
        int i;

        for (i=0; i<cfg->nb_threads; i++) {
                if (!is_thread_active(i))
                        continue;

                proxenet_join_thread(i);
	}

        return;
}


/**
 * This function is called when proxenet is loaded. Therefore, only the scripts
 * located in the `autoload_path` must be loaded.
 * Additionnal plugins can be added via the command line client.
 *
 * @return 0 on success
 * @return -1 on error
 */
int proxenet_initialize_plugins_list()
{
        /* NULL as 2nd arg means to try to add all valid plugins from the path */
        if(proxenet_add_new_plugins(cfg->autoload_path, NULL) < 0) {
                xlog(LOG_CRITICAL, "%s\n", "Failed to build plugins list, leaving");
                return -1;
        }

        if(cfg->verbose) {
                xlog(LOG_INFO, "%s\n", "Plugins loaded");
                if (cfg->verbose > 1) {
                        xlog(LOG_INFO, "%d plugin(s) found\n", proxenet_plugin_list_size());
                        proxenet_print_plugins_list(-1);
                }
        }

        return 0;
}


/**
 * Returns the next valid index for a new thread
 *
 * @return the index of the available thread in the bitmask, -1 if no available
 */
int get_new_thread_id()
{
        int tnum;

        for(tnum=0; is_thread_active(tnum) && tnum<cfg->nb_threads; tnum++);
        return (tnum > cfg->nb_threads) ? -1 : tnum;
}


/**
 * This function is the main loop for the main thread. It does the following:
 * - prepares the structures, setup the signal handlers and changes the state
 *   of proxenet to active
 * - starts the big loop
 *   - listen and accept new connection
 *   - allocate a new thread id if possible
 *   - and starts the new thread with the new fd
 * - listens for incoming events on the control socket
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
        proxy_state = ACTIVE;

        /* big loop  */
        while (proxy_state != INACTIVE) {
                FD_ZERO(&sock_set);

                FD_SET(sock, &sock_set);
                FD_SET(ctl_sock, &sock_set);
                if (ctl_cli_sock > 0)
                        FD_SET(ctl_cli_sock, &sock_set);

                purge_zombies();

                /* set asynchronous listener */
                struct timespec timeout = {
                        .tv_sec = HTTP_TIMEOUT_SOCK,
                        .tv_nsec = 0
                };

                retcode = pselect(FD_SETSIZE, &sock_set, NULL, NULL, &timeout, &oldmask);

                if (retcode < 0) {
                        if (errno != EINTR) {
                                xlog(LOG_ERROR, "[main] pselect() returned %d: %s\n", retcode, strerror(errno));
                                proxy_state = INACTIVE;
                                break;
                        } else {
                                continue;
                        }
                }

                if (retcode == 0)
                        continue;

                if (proxy_state == INACTIVE)
                        break;

                /* event on the listening socket -> new request */
                if( FD_ISSET(sock, &sock_set) && proxy_state != SLEEPING) {
#ifdef DEBUG
                        xlog(LOG_DEBUG, "New event on proxy sock=#%d\n", sock);
#endif

                        tid = get_new_thread_id();
                        if(tid < 0) {
#ifdef DEBUG
                                xlog(LOG_DEBUG, "Thread pool exhausted, cannot proceed with sock=#%d\n", sock);
#endif
                                continue;
                        }

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
                        xlog(LOG_DEBUG, "New event on control ctl_sock=#%d\n", ctl_sock);
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
        xlog(LOG_DEBUG, "Received signal %s\n", strsignal(signum));
#endif

        switch(signum) {

                case SIGTERM:
                case SIGINT:
			proxy_state = INACTIVE;

                        cfg->try_exit++;
                        xlog(LOG_INFO, "%s, %d/%d\n", "Trying to leave", cfg->try_exit, cfg->try_exit_max);

                        if (cfg->try_exit == cfg->try_exit_max) {
                                xlog(LOG_CRITICAL, "%s\n", "Failed to exit properly");
                                abort();
                        }

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
        proxenet_xzero(saction, sizeof(struct sigaction));

        saction->sa_handler = sighandler;
        saction->sa_flags = SA_RESTART|SA_NOCLDSTOP;
        sigemptyset(&(saction->sa_mask));

        sigaction(SIGINT,  saction, NULL);
        sigaction(SIGTERM, saction, NULL);
        sigaction(SIGCHLD, saction, NULL);

        saction->sa_handler = SIG_IGN;
        sigaction(SIGPIPE,  saction, NULL);

        return;
}


/**
 *
 */
static inline void init_global_stats()
{
        bytes_sent = 0;
        bytes_recv = 0;
        start_time = time(NULL);
        return;
}


/**
 *
 */
static inline void end_global_stats()
{
        end_time = time(NULL);
        return;
}


/**
 * Prints a few information about the proxenet session.
 */
static void print_global_stats()
{
        struct tm *t1, *t2;
        char buf[128];
        float bytes_sent_kb, bytes_sent_mb;
        float bytes_recv_kb, bytes_recv_mb;

        t1 = localtime(&start_time);
        t2 = localtime(&end_time);


        xlog(LOG_INFO, "%s\n", "Statistics:");

        proxenet_xzero(buf, sizeof(buf));
        strftime(buf, sizeof(buf), "%F %T", t1);
        xlog(LOG_INFO, "Start time: %s\n", buf);

        proxenet_xzero(buf, sizeof(buf));
        strftime(buf, sizeof(buf), "%F %T", t2);
        xlog(LOG_INFO, "End time: %s\n", buf);

        xlog(LOG_INFO, "Session duration: %lu seconds\n", (end_time-start_time));

        bytes_sent_kb = bytes_sent/1024.0;
        bytes_sent_mb = bytes_sent_kb/1024.0;

        xlog(LOG_INFO, "Number of bytes sent: %luB (%.2fkB, %.2fMB)\n",
             bytes_sent, bytes_sent_kb, bytes_sent_mb);

        bytes_recv_kb = bytes_recv/1024.0;
        bytes_recv_mb = bytes_recv_kb/1024.0;

        xlog(LOG_INFO, "Number of bytes received: %luB (%.2fkB, %5.2fMB)\n",
             bytes_recv,bytes_recv_kb, bytes_recv_mb);

        xlog(LOG_INFO, "Number of unique requests: %lu\n", (request_id-1));

        return;
}


/**
 * This function is called right after the configuration was parsed.
 * It simply:
 * - creates the main listening sockets (control and proxy)
 * - initialize the signal mask
 * - initialize all the VMs
 * - fill the plugin list with the valid plugins located in the autoload path
 * - then call the main thread loop
 * - once finished, it also cleans the structures
 *
 * @return 0 if everything went well, -1 otherwise with an error message
 */
int proxenet_start()
{
        sock_t control_socket, listening_socket;
        struct sigaction saction;

        /* create control socket */
        control_socket = create_control_socket();
        if (control_socket < 0) {
                xlog(LOG_CRITICAL, "Cannot create control socket: %s\n", strerror(errno));
                return -1;
        }

        if(cfg->verbose)
                xlog(LOG_INFO, "Control socket: %d\n", control_socket);

        /* create listening socket */
        listening_socket = create_bind_socket(cfg->iface, cfg->port);
        if (listening_socket < 0) {
                xlog(LOG_CRITICAL, "Cannot create bind socket: %s\n", strerror(errno));
                return -1;
        }

        if(cfg->verbose)
                xlog(LOG_INFO, "Bind socket: %d\n", listening_socket);


        /* init everything */
        initialize_sigmask(&saction);

        plugins_list = NULL;
        proxy_state = INACTIVE;
        active_threads_bitmask = 0;

        /* set up plugins */
        if( proxenet_initialize_plugins_list() < 0 )
                return -1;

        /* this call *MUST* succeed or die */
        proxenet_initialize_plugins();

        /* setting request counter  */
        request_id = 0;

        /* we "artificially" allocate an ID 0 so that all new requests will be > 0 */
        /* for the child threads, a request id of 0 means not allocated */
        get_new_request_id();

        init_global_stats();

        /* prepare threads and start looping */
        xloop(listening_socket, control_socket);

        end_global_stats();

        if (cfg->verbose)
                print_global_stats();

        /* clean context */
        proxenet_destroy_plugins_vm();
        proxenet_free_all_plugins();

        close_socket(listening_socket);
        close_socket(control_socket);

        unlink(CONTROL_SOCK_PATH);
        return 0;
}

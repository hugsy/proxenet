#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <alloca.h>
#include <netdb.h>
#include <stdlib.h>

#include "control-server.h"
#include "core.h"
#include "socket.h"
#include "utils.h"

#define BUFSIZE 2048


static struct command_t known_commands[] = {
	{ "quit", 	 0, &quit_cmd, "Make "PROGNAME" leave kindly" },
	{ "help", 	 0, &help_cmd, "Show this menu" },
	{ "pause", 	 0, &pause_cmd, "Toggle pause" },
	{ "info", 	 0, &info_cmd, "Display information about environment" },
	{ "verbose", 	 1, &verbose_cmd, "Get/Set verbose level"},
	{ "reload", 	 0, &reload_cmd, "Reload the plugins" },
	{ "threads", 	 0, &threads_cmd, "Show info about threads" },
	{ "plugin", 	 1, &plugin_cmd, "Get/Set info about plugin"},

	{ NULL, 0, NULL, NULL}
};


/**
 * Send detailed information to socket
 *
 * @param fd the socket to send data to
 */
static void _send_threads_info(sock_t fd)
{
        char msg[BUFSIZE] = {0,};
        int i, n;

        n = snprintf(msg, sizeof(msg),
                     "- Running/Max threads: %d/%d\n",
                     get_active_threads_size(),
                     cfg->nb_threads);
        proxenet_write(fd, (void*)msg, n);

        for (i=0; i < cfg->nb_threads; i++) {
                if (!is_thread_active(i)) continue;
                memset(msg, 0, sizeof(msg));
                n = snprintf(msg, sizeof(msg), "\t- Thread %d (%lu)\n", i, threads[i]);
                proxenet_write(fd, (void*)msg, n);
        }

        return;
}


/**
 * This command will try to kill gracefully the running process of proxenet
 */
void quit_cmd(sock_t fd, char *options, unsigned int nb_options)
{
        char *msg = "Leaving gracefully\n";

        /* happy compiler means karma++ */
        (void) options;
        (void) nb_options;

        proxenet_write(fd, (void*)msg, strlen(msg));
        proxy_state = INACTIVE;
        return;
}


/**
 * The famous help menu
 */
void help_cmd(sock_t fd, char *options, unsigned int nb_options)
{
        struct command_t *cmd;
        char *msg;
        unsigned int msglen = 20 + 80 + 3;

        (void) options;
        (void) nb_options;

        msg = "Command list:\n";
        proxenet_write(fd, (void*)msg, strlen(msg));

        for (cmd=known_commands; cmd && cmd->name; cmd++) {
                msg = alloca(msglen+1);
                proxenet_xzero(msg, msglen+1);
                snprintf(msg, msglen+1, "%-15s\t%s\n", cmd->name, cmd->desc);
                proxenet_write(fd, (void*)msg, strlen(msg));
        }

        return;
}


/**
 * (De)-Activate pause mode (suspend and block interception)
 */
void pause_cmd(sock_t fd, char *options, unsigned int nb_options)
{
        char *msg;

        (void) options;
        (void) nb_options;

        if (proxy_state==SLEEPING) {
                msg = "sleep-mode -> 0\n";
                proxy_state = ACTIVE;
        } else {
                msg = "sleep-mode -> 1\n";
                proxy_state = SLEEPING;
        }

        xlog(LOG_INFO, "%s", msg);
        proxenet_write(fd, (void*)msg, strlen(msg));
        return;
}


/**
 * Get information about proxenet state.
 */
void info_cmd(sock_t fd, char *options, unsigned int nb_options)
{
        char msg[BUFSIZE] = {0, };
        int n;

        (void) options;
        (void) nb_options;

        /* generic info  */
        n = snprintf(msg, sizeof(msg),
                     "Configuration:\n"
                     "- Listening interface: %s/%s\n"
                     "- Supported IP version: %s\n"
                     "- Logging to %s\n"
                     "- SSL private key: %s\n"
                     "- SSL certificate: %s\n"
                     "- Proxy: %s [%s]\n"
                     "- Plugins directory: %s\n"
                     "- Autoloading plugins directory: %s\n"
                     "- Number of requests treated: %lu\n"
                     ,
                     cfg->iface, cfg->port,
                     (cfg->ip_version==AF_INET)? "IPv4": (cfg->ip_version==AF_INET6)?"IPv6": "ANY",
                     (cfg->logfile)?cfg->logfile:"stdout",
                     cfg->keyfile,
                     cfg->cafile,
                     cfg->proxy.host ? cfg->proxy.host : "None",
                     cfg->proxy.host ? cfg->proxy.port : "direct",
                     cfg->plugins_path,
                     cfg->autoload_path,
                     (request_id-1)
                    );
        proxenet_write(fd, (void*)msg, n);

        /* threads info  */
        _send_threads_info(fd);

        /* plugins info */
        if (proxenet_plugin_list_size()) {
                proxenet_print_plugins_list(fd);
        } else {
                proxenet_write(fd, (void*)"No plugin loaded\n", 17);
        }

        return;
}


/**
 * Increase/Decrease verbosity of proxenet (useful for logging/debugging)
 */
void verbose_cmd(sock_t fd, char *options, unsigned int nb_options)
{
        char msg[BUFSIZE] = {0, };
        char *ptr;
        int n;

        (void) options;
        (void) nb_options;

        ptr = strtok(options, " \n");
        if (!ptr){
                n = snprintf(msg, BUFSIZE, "Verbose level is at %d\n", cfg->verbose);
                proxenet_write(fd, (void*)msg, n);
                return;
        }

        if (strcmp(ptr, "inc")==0 && cfg->verbose<MAX_VERBOSE_LEVEL)
                n = snprintf(msg, BUFSIZE, "Verbose level is now %d\n", ++cfg->verbose);
        else if (strcmp(ptr, "dec")==0 && cfg->verbose>0)
                n = snprintf(msg, BUFSIZE, "Verbose level is now %d\n", --cfg->verbose);
        else
                n = snprintf(msg, BUFSIZE, "Invalid action\n Syntax\n verbose (inc|dec)\n");

        proxenet_write(fd, (void*)msg, n);

        return;
}


/**
 * Reload proxenet
 */
void reload_cmd(sock_t fd, char *options, unsigned int nb_options)
{
        char *msg;

        (void) options;
        (void) nb_options;

        if (get_active_threads_size() > 0) {
                msg = "Threads still active, cannot reload";
                proxenet_write(fd, (void*)msg, strlen(msg));
                return;
        }

        proxy_state = SLEEPING;

        proxenet_destroy_plugins_vm();
        proxenet_remove_all_plugins();

        if( proxenet_initialize_plugins_list() < 0) {
                msg = "Failed to reinitilize plugins";
                proxenet_write(fd, (void*)msg, strlen(msg));
                proxy_state = INACTIVE;
                return;
        }

        proxenet_initialize_plugins();

        proxy_state = ACTIVE;

        msg = "Plugins list successfully reloaded\n";
        proxenet_write(fd, (void*)msg, strlen(msg));

        return;
}


/**
 * Get information about the threads
 */
void threads_cmd(sock_t fd, char *options, unsigned int nb_options)
{
        char msg[BUFSIZE] = {0, };
        char *ptr;
        int n;

        (void) options;
        (void) nb_options;

        ptr = strtok(options, " \n");
        if (!ptr){
                _send_threads_info(fd);
                return;
        }

        if (strcmp(ptr, "inc")==0 && cfg->nb_threads<MAX_THREADS)
                n = snprintf(msg, BUFSIZE, "Nb threads level is now %d\n", ++cfg->nb_threads);
        else if (strcmp(ptr, "dec")==0 && cfg->nb_threads>1)
                n = snprintf(msg, BUFSIZE, "Nb threads level is now %d\n", --cfg->nb_threads);
        else
                n = snprintf(msg, BUFSIZE, "Invalid action\n Syntax\n threads (inc|dec)\n");

        proxenet_write(fd, (void*)msg, n);

        return;
}


/**
 * Handle plugins (list/activate/deactivate/load)
 */
void plugin_cmd(sock_t fd, char *options, unsigned int nb_options)
{
        char msg[BUFSIZE] = {0, };
        char *ptr;
        int n, res;
        unsigned int plugin_id;
        char* plugin_name = NULL;

        (void) options;
        (void) nb_options;


        /* usage */
        ptr = strtok(options, " \n");
        if (!ptr)
                goto invalid_plugin_action;


        /* list loaded */
        if (strcmp(ptr, "list") == 0) {
                proxenet_print_plugins_list(fd);
                return;
        }

        /* list all plugins in plugin directory */
        if (strcmp(ptr, "list-all") == 0) {
                proxenet_print_all_plugins(fd);
                return;
        }

        /* (de-)activate a plugin based on its id */
        if (strcmp(ptr, "toggle") == 0) {
                ptr = strtok(NULL, " \n");
                if (!ptr) {
                        n = sprintf(msg, "Missing plugin id\n");
                        proxenet_write(fd, (void*)msg, n);
                        return;
                }

                plugin_id = atoi(ptr);
                if (0 < plugin_id && plugin_id <= proxenet_plugin_list_size() ) {
                        res = proxenet_toggle_plugin(plugin_id);
                        if (res < 0){
                                n = snprintf(msg, BUFSIZE, "Failed to toggle plugin %d\n", plugin_id);
                                proxenet_write(fd, (void*)msg, n);
                                return;
                        }
                        n = snprintf(msg, BUFSIZE, "Plugin %d is now %sACTIVE\n", plugin_id, res?"":"IN");
                        proxenet_write(fd, (void*)msg, n);
                        return;
                }
        }

        /* add a plugin at runtime */
        if (strcmp(ptr, "load") == 0) {
                ptr = strtok(NULL, " \n");
                if (!ptr){
                        proxenet_write(fd, (void*)"Missing plugin name\n", 20);
                        return;
                }

                if ( (plugin_name=proxenet_xstrdup2(ptr)) == NULL ){
                        n = sprintf(msg, "proxenet_xstrdup2() failed: %s\n", strerror(errno));
                        proxenet_write(fd, (void*)msg, n);
                        return;
                }

                res = proxenet_add_new_plugins(cfg->plugins_path, plugin_name);
                if(res<0){
                        n = snprintf(msg, sizeof(msg)-1, "Error while loading plugin '%s'\n", plugin_name);
                        proxenet_write(fd, (void*)msg, n);
                }else{
                        if(res)
                                n = snprintf(msg, sizeof(msg)-1, "Plugin '%s' added successfully\n", plugin_name);
                        else
                                n = snprintf(msg, sizeof(msg)-1, "File '%s' has not been added\n", plugin_name);

                        proxenet_write(fd, (void*)msg, n);
                }

                /* plugin list must be reinitialized since we dynamically load VMs only if plugins are found */
                proxenet_initialize_plugins();

                proxenet_xfree(plugin_name);
                return;
        }



invalid_plugin_action:
        n = snprintf(msg, BUFSIZE, "Invalid action\nSyntax\n plugin [list][list-all]|[toggle <num>][load <0PluginName.ext>]\n");
        proxenet_write(fd, (void*)msg, n);
        return;
}


/**
 * Parse an incoming command
 *
 * @param name is a pointer to the buffer of the command to parse
 * @return pointer to valid struct command_t if `name` is found as known_commands[]
 * @return NULL otherwise
 */
struct command_t* get_command(char *name)
{
        struct command_t *cmd;
        char *buf = name;
        char c;

        do {
                c = *buf;
                if (c == '\0')
                        return NULL;
                if (c=='\n' || c==' ') {
                        *buf = '\0';
                        break;
                }

                buf++;
        } while (1);

        for (cmd=known_commands; cmd && cmd->name; cmd++) {
                if (strcmp(name, cmd->name) == 0) {
                        *buf = c;
                        return cmd;
                }
        }

        return NULL;
}


/**
 * Main handler for new control command
 */
int proxenet_handle_control_event(sock_t* sock) {
        char read_buf[BUFSIZE] = {0, };
        char *ptr = NULL;
        int retcode = -1;
        struct command_t *cmd = NULL;

        retcode = proxenet_read(*sock, read_buf, BUFSIZE-1);
        if (retcode < 0) {
                xlog(LOG_ERROR, "Failed to read control command: %s\n", strerror(errno));
                return -1;
        }

        if (retcode == 0) {
                return -1;
        }

        if (read_buf[0] == '\n') {
                goto cmd_end;
        }

        if ( (cmd=get_command(read_buf)) == NULL ) {
                proxenet_write(*sock, CONTROL_INVALID, strlen(CONTROL_INVALID));
                goto cmd_end;
        }

        if(cfg->verbose)
                xlog(LOG_INFO, "Receiving control command: \"%s\" \n", cmd->name);

        ptr = &read_buf[strlen(cmd->name)];
        cmd->func(*sock, ptr, cmd->nb_opt_max);

cmd_end:
        proxenet_write(*sock, CONTROL_PROMPT, strlen(CONTROL_PROMPT));

        return 0;
}

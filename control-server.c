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

#define BUFSIZE 1024


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
 *
 */
void quit_cmd(sock_t fd, char *options, unsigned int nb_options)
{
	char *msg = "Leaving gracefully\n";
	proxenet_write(fd, (void*)msg, strlen(msg));
	proxenet_state = INACTIVE;

	return;
}


/**
 *
 */
void help_cmd(sock_t fd, char *options, unsigned int nb_options)
{
	struct command_t *cmd;
	char *msg;
	unsigned int msglen = 20 + 80 + 3;

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
 *
 */
void pause_cmd(sock_t fd, char *options, unsigned int nb_options)
{
	char *msg;
	if (proxenet_state==SLEEPING) {
		msg = "sleep-mode -> 0\n";
		proxenet_state = ACTIVE;
	} else {
		msg = "sleep-mode -> 1\n";
		proxenet_state = SLEEPING;
	}

	xlog(LOG_INFO, "%s", msg);
	proxenet_write(fd, (void*)msg, strlen(msg));
	return;
}


/**
 *
 */
void info_cmd(sock_t fd, char *options, unsigned int nb_options)
{
	char msg[1024] = {0, };
	char *msg2 = NULL;
	
	snprintf(msg, 1024,
		 "Infos:\n"
		 "- Listening interface: %s/%s\n"
		 "- Supported IP version: %s\n"
		 "- Logging to %s\n"
		 "- Running/Max threads: %d/%d\n"
		 "- SSL private key: %s\n"
		 "- SSL certificate: %s\n"
		 "- Proxy: %s [%s]\n"
		 "- Plugins directory: %s\n"
		 , 
		 cfg->iface, cfg->port,
		 (cfg->ip_version==AF_INET)? "IPv4": (cfg->ip_version==AF_INET6)?"IPv6": "ANY",
		 (cfg->logfile)?cfg->logfile:"stdout",
		 get_active_threads_size(), cfg->nb_threads,
		 cfg->keyfile,
		 cfg->certfile,
		 cfg->proxy.host ? cfg->proxy.host : "None",
		 cfg->proxy.host ? cfg->proxy.port : "direct",
		 cfg->plugins_path  
		);

	proxenet_write(fd, (void*)msg, strlen(msg));
	
	if (proxenet_plugin_list_size()) {
		msg2 = proxenet_build_plugins_list();
		proxenet_write(fd, (void*)msg2, strlen(msg2));
		proxenet_xfree(msg2);

	} else {
		proxenet_write(fd, (void*)"No plugin loaded\n", 17);
	}
	
	return;
}


/**
 *
 */
void verbose_cmd(sock_t fd, char *options, unsigned int nb_options)
{
	char msg[1024] = {0, };
	char *ptr;
	int n;

	ptr = strtok(options, " \n");
	if (!ptr){
		n = snprintf(msg, 1024, "Verbose level is at %d\n", cfg->verbose);
		proxenet_write(fd, (void*)msg, n);
		return;
	}

	if (strcmp(ptr, "inc")==0 && cfg->verbose<5)
		n = snprintf(msg, 1024, "Verbose level is now %d\n", ++cfg->verbose);
	else if (strcmp(ptr, "dec")==0 && cfg->verbose>0)
		n = snprintf(msg, 1024, "Verbose level is now %d\n", --cfg->verbose);
	else
		n = snprintf(msg, 1024, "Invalid action\n Syntax\n verbose (inc|dec)\n");

	proxenet_write(fd, (void*)msg, n);

	return;
}


/**
 *
 */
void reload_cmd(sock_t fd, char *options, unsigned int nb_options)
{
	char *msg;
	
	if (get_active_threads_size() > 0) {
		msg = "Threads still active, cannot reload";
		proxenet_write(fd, (void*)msg, strlen(msg));
		return;
	}

	proxenet_state = SLEEPING;
	
	proxenet_destroy_plugins_vm();
	proxenet_delete_list_plugins();

	if( proxenet_initialize_plugins_list() < 0) {
		msg = "Failed to reinitilize plugins";
		proxenet_write(fd, (void*)msg, strlen(msg));
		proxenet_state = INACTIVE;
		return;
	}

	proxenet_initialize_plugins();

	proxenet_state = ACTIVE;

	msg = "Plugins list successfully reloaded\n";
	proxenet_write(fd, (void*)msg, strlen(msg));

	return;
}


/**
 *
 */
void threads_cmd(sock_t fd, char *options, unsigned int nb_options)
{
	char msg[1024] = {0, };
	char *ptr;
	int n;

	ptr = strtok(options, " \n");
	if (!ptr){
		n = get_active_threads_size();
		n = snprintf(msg, 128, "%d active thread%c/%d max thread%c\n",
			     n, (n>1)?'s':' ',
			     cfg->nb_threads, (cfg->nb_threads>1)?'s':' '
			    );
		proxenet_write(fd, (void*)msg, n);
		return;
	}

	if (strcmp(ptr, "inc") == 0)
		n = snprintf(msg, 1024, "Nb threads level is now %d\n", ++cfg->nb_threads);
	else if (strcmp(ptr, "dec") == 0)
		n = snprintf(msg, 1024, "Nb threads level is now %d\n", --cfg->nb_threads);
	else
		n = snprintf(msg, 1024, "Invalid action\n Syntax\n threads (inc|dec)\n");

	proxenet_write(fd, (void*)msg, n);
	
	return;
}


/**
 *
 */
void plugin_cmd(sock_t fd, char *options, unsigned int nb_options)
{
	char msg[1024] = {0, };
	char *ptr, *plist_str;
	int n, res;
	
	ptr = strtok(options, " \n");
	if (!ptr){
		n = snprintf(msg, 1024, "Invalid action\nSyntax\n plugin [list]|[toggle <num>]\n");
		proxenet_write(fd, (void*)msg, n);
		return;
	}

	if (strcmp(ptr, "list") == 0) {
		plist_str = proxenet_build_plugins_list();
		proxenet_write(fd, (void*)plist_str, strlen(plist_str));
		proxenet_xfree(plist_str);
		return;
		
	} else if (strcmp(ptr, "toggle") == 0) {
		ptr = strtok(NULL, " \n");
		if (!ptr)
			return;

		n = atoi(ptr);
		if (0 < n && n <= proxenet_plugin_list_size() ) {
			res = proxenet_toggle_plugin(n);
			n = snprintf(msg, 1024, "Plugin %d is now %sACTIVE\n", n, res?"":"IN");
			proxenet_write(fd, (void*)msg, n);
			return;
		}
	}
	
	n = snprintf(msg, 1024, "Invalid action\nSyntax\n plugin [list]|[toggle <num>]\n");
	proxenet_write(fd, (void*)msg, n);
	return;
}


/**
 *
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
 *
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
	
#ifdef DEBUG
	xlog(LOG_DEBUG, "Receiving control command: \"%s\" \n", cmd->name);
#endif

	ptr = &read_buf[strlen(cmd->name)];
	cmd->func(*sock, ptr, cmd->nb_opt_max);

cmd_end:
	proxenet_write(*sock, CONTROL_PROMPT, strlen(CONTROL_PROMPT));
	
	return 0;
}


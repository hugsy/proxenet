#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <alloca.h>
#include <netdb.h>

#include "control-server.h"
#include "core.h"
#include "socket.h"
#include "utils.h"

#define BUFSIZE 1024


static struct command_t known_commands[] = {
	{ "quit", 	 0, &quit_cmd, "Leave kindly" },
	{ "help", 	 0, &help_cmd, "Show this menu" },
	{ "pause", 	 0, &pause_cmd, "Toggle pause" },
	{ "info", 	 0, &info_cmd, "Display information about environment" },
	{ "verbose", 	 1, &verbose_cmd, "Get/Set verbose level"},
	{ "reload", 	 0, &reload_cmd, "Reload the plugins" },
	{ "threads", 	 0, &threads_cmd, "Show info about threads" },
	/* { "plugin", 	 1, &plugin_cmd, "Get/Set info about plugin"}, */
	
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
		 cfg->iface,
		 cfg->port,
		 (cfg->ip_version==AF_INET)? "IPv4": (cfg->ip_version==AF_INET6)?"IPv6": "ANY",
		 (cfg->logfile)?cfg->logfile:"stdout",
		 get_active_threads_size(),
		 cfg->nb_threads,					     
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

	ptr = strtok(options, " ");
	if (!ptr){
		n = snprintf(msg, 1024, "Verbose level is at %d\n", cfg->verbose);
		proxenet_write(fd, (void*)msg, n);
		return;
	}

	if (strcmp(ptr, "inc") == 0) 
		n = snprintf(msg, 1024, "Verbose level is now %d\n", ++cfg->verbose);
	else if (strcmp(ptr, "dec") == 0)
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
	proxenet_delete_list_plugins();
					
	if( proxenet_initialize_plugins_list() < 0) {
		msg = "Failed to reinitilize plugins";
		proxenet_write(fd, (void*)msg, strlen(msg));
		proxenet_state = INACTIVE;
		return;
	}

	proxenet_destroy_plugins_vm();
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
	char msg[128] = {0, };
	long i;

	i = get_active_threads_size();
	i = snprintf(msg, 128, "%ld active thread%c\n", i, (i>1)?'s':' ');
	proxenet_write(fd, (void*)msg, i);
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
	
	for (cmd=known_commands; cmd && cmd->name; cmd++) {
		if (strcmp(name, cmd->name) == 0)
			return cmd;
	}
	
	return NULL;
}

	
/**
 *
 */
void proxenet_handle_control_event(sock_t* sock) {
	char read_buf[BUFSIZE] = {0, };
	char *ptr = NULL;
	int retcode = -1;
	struct command_t *cmd = NULL;
	
	retcode = proxenet_read(*sock, read_buf, BUFSIZE-1);
	if (retcode < 0) {
		xlog(LOG_ERROR, "Failed to read control command: %s\n", strerror(errno));
		return;
	}

	ptr = index(read_buf, '\n');
	if (ptr)
		*ptr = '\0';
	
#ifdef DEBUG
	xlog(LOG_DEBUG, "Receiving control command: \"%s\"\n", read_buf);
#endif
	ptr = strtok(read_buf, " ");
	if ( (cmd=get_command(ptr)) == NULL ) {
		proxenet_write(*sock, CONTROL_INVALID, strlen(CONTROL_INVALID));
		proxenet_write(*sock, CONTROL_PROMPT, strlen(CONTROL_PROMPT));
		return;
	}
	
	cmd->func(*sock, strtok(NULL, " "), cmd->nb_opt_max);

	proxenet_write(*sock, CONTROL_PROMPT, strlen(CONTROL_PROMPT));
	return;
}

			/*

				case 'v':
					if (cfg->verbose < MAX_VERBOSE_LEVEL)
					xlog(LOG_INFO, "Verbosity is now %d\n", ++(cfg->verbose));
					break;
					
				case 'b':
					if (cfg->verbose > 0)
						xlog(LOG_INFO, "Verbosity is now %d\n", --(cfg->verbose));
					break;
					

					
				default:
					if (cmd >= '1' && cmd <= '9') {
						proxenet_toggle_plugin(cmd-0x30);
						break;
					}
					
			

			*/

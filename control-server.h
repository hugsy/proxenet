#ifndef _CONTROL_SERVER_H
#define _CONTROL_SERVER_H

#include "core.h"
#include "socket.h"

#define MAX_CMD_LEN 1024

#define CONTROL_SOCK_PATH "/tmp/proxenet-control-socket"
#define CONTROL_MOTD "Welcome on "PROGNAME" control interface\nType `help` to list available commands\n"
#define CONTROL_PROMPT ">>> "
#define CONTROL_INVALID "Invalid command\n"

struct command_t {
		char *name;
		unsigned int nb_opt_max;
		void (*func)(sock_t, char *options, unsigned int nb_options);
		const char *desc;
};

void quit_cmd(sock_t  fd, char *options, unsigned int nb_options);
void help_cmd(sock_t fd, char *options, unsigned int nb_options);
void pause_cmd(sock_t fd, char *options, unsigned int nb_options);
void info_cmd(sock_t fd, char *options, unsigned int nb_options);
void verbose_cmd(sock_t fd, char *options, unsigned int nb_options);
void reload_cmd(sock_t fd, char *options, unsigned int nb_options);
void threads_cmd(sock_t fd, char *options, unsigned int nb_options);
void plugin_cmd(sock_t fd, char *options, unsigned int nb_options);
void config_cmd(sock_t fd, char *options, unsigned int nb_options);

int proxenet_handle_control_event(sock_t*);

#endif /* _CONTROL_SERVER_H */

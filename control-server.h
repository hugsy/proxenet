#ifndef _CONTROL_SERVER_H
#define _CONTROL_SERVER_H

#include "core.h"
#include "socket.h"

#define MAX_CMD_LEN 1024

#define CONTROL_SOCK_PATH "/tmp/proxenet-control-socket"
#define CONTROL_MOTD "Welcome on "PROGNAME" control interface\nType `help` to list available commands\n"
#define CONTROL_PROMPT ">>> "
#define CONTROL_INVALID "{\"error\": \"Invalid command\"}"

struct command_t {
		char *name;
		unsigned int nb_opt_max;
		void (*func)(sock_t, char *options, unsigned int nb_options);
		const char *desc;
};

int proxenet_handle_control_event(sock_t*);

#endif /* _CONTROL_SERVER_H */

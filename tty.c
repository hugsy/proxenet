/*
 *
 * non-blocking tty
 * 
 */
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "tty.h"
#include "utils.h"


struct termios saved_ti;

void tty_open()
{
	struct termios ti;
	int flags = O_RDONLY | O_NONBLOCK;
	
	if (tty_fd) return;
	
	if ((tty_fd = open("/dev/tty", flags)) < 0) return;
	
	tcgetattr(tty_fd, &ti);
	saved_ti = ti;
	ti.c_lflag &= ~(ICANON | ECHO);
	ti.c_cc[VMIN] = 1;
	ti.c_cc[VTIME] = 0;
	tcsetattr(tty_fd, TCSANOW, &ti);
}


char tty_getc()
{
	int retcode;
	char c;
	
	if (tty_fd) {
		c = 0;
		retcode = read(tty_fd, &c, 1);
		if (retcode == 1)
			return c;
		else
			xlog(LOG_ERROR, "Failed to read character from tty: %s\n", strerror(errno));
	}
	
	return -1;
}


void tty_close()
{
	if (!tty_fd) return;
	
	tcsetattr(tty_fd, TCSANOW, &saved_ti);
	
	close(tty_fd);
	tty_fd = 0;
}


void tty_flush()
{
	tcflush(tty_fd, TCIFLUSH);
}



#ifndef _TTY_H
#define _TTY_H

int tty_fd;

char tty_getc();
void tty_open();
void tty_close();
void tty_flush();

#endif

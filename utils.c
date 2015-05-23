#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "core.h"
#include "utils.h"


static pthread_mutex_t tty_mutex;


/**
 *
 */
void _xlog(int type, const char* fmt, ...)
{
	va_list ap;

	/* lock tty before printing */
        pthread_mutex_lock(&tty_mutex);

	switch (type) {
		case LOG_CRITICAL:
			if (cfg->use_color) fprintf(cfg->logfile_fd, DARK);
			fprintf(cfg->logfile_fd, "CRITICAL: ");
			break;

		case LOG_ERROR:
			if (cfg->use_color) fprintf(cfg->logfile_fd, RED);
			fprintf(cfg->logfile_fd, "ERROR: ");
			break;

		case LOG_WARNING:
			if (cfg->use_color) fprintf(cfg->logfile_fd, YELLOW);
			fprintf(cfg->logfile_fd, "WARNING: ");
			break;

		case LOG_DEBUG:
			if (cfg->use_color) fprintf(cfg->logfile_fd, BLUE);
			fprintf(cfg->logfile_fd, "DEBUG: ");
			break;

		case LOG_INFO:
		default:
			if (cfg->use_color) fprintf(cfg->logfile_fd, GREEN);
			fprintf(cfg->logfile_fd, "INFO: ");
			break;
	}

	if (cfg->use_color) fprintf(cfg->logfile_fd, NOCOLOR);


#ifdef DEBUG
	fprintf(cfg->logfile_fd, "tid-%lu ", pthread_self());
#endif

	va_start(ap, fmt);
	vfprintf(cfg->logfile_fd, fmt, ap);
	fflush(cfg->logfile_fd);
	va_end(ap);

	/* release lock */
        pthread_mutex_unlock(&tty_mutex);
}



/**
 * malloc(3) wrapper. Checks size and zero-fill buffer.
 *
 * Note: (re-)allocation is a "succeed-or-die" process in proxenet.
 *
 * @param size: buffer size to allocate on heap
 * @return ptr: allocated zero-filled buffer pointer
 */
void* proxenet_xmalloc(size_t size)
{
	void *ptr;

	if (size > SIZE_MAX / sizeof(size_t)) {
		xlog(LOG_CRITICAL, "proxenet_xmalloc: try to allocate incorrect size (%d byte)\n", size);
		abort();
	}

	ptr = malloc(size);
	if ( ptr == NULL ) {
		xlog(LOG_CRITICAL, "%s\n", "proxenet_xmalloc: fail to allocate space");
		abort();
	}

	proxenet_xzero(ptr, size);
	return ptr;
}


/**
 * Free allocated blocks. Forcing abort() to generate a coredump.
 *
 * @param ptr: pointer to zone to free
 */
void proxenet_xfree(void* ptr)
{
	if(ptr == NULL) {
		xlog(LOG_CRITICAL, "%s\n", "Trying to free NULL pointer");
		abort();
	}

	free(ptr);
        return;
}


/**
 * realloc(3) wrapper. Checks size and zero-fill buffer.
 *
 * @param oldptr: pointer to previous area
 * @param new_size: new size to allocate
 * @return a pointer to resized pointer
 */
void* proxenet_xrealloc(void* oldptr, size_t new_size)
{
	void *newptr;

	if (new_size > (SIZE_MAX / sizeof(size_t))) {
		xlog(LOG_CRITICAL, "proxenet_xrealloc: try to allocate incorrect size (%d byte)\n", new_size);
		abort();
	}

	newptr = realloc(oldptr, new_size);
	if (newptr == NULL) {
		xlog(LOG_CRITICAL, "proxenet_xrealloc() failed to allocate space: %s\n", strerror(errno));
		abort();
	}

	return newptr;
}


/**
 * Fill buflen-sized buffer with zeroes
 *
 * @param buf : buffer to zero-ize
 * @param buflen : buf length
 */
void proxenet_xzero(void* buf, size_t buflen)
{
	if (!buf) {
		xlog(LOG_CRITICAL, "Trying to zero-ify NULL pointer %p\n", buf);
		abort();
	}

	memset(buf, 0, buflen);
}


/**
 * Clean heap allocated block, and free it.
 *
 * @param buf : buffer to zero-ize
 * @param buflen : buf length
 */
void proxenet_xclean(void* buf, size_t buflen)
{
        proxenet_xzero(buf, buflen);
        proxenet_xfree(buf);
        return;
}


/**
 * Wrapper for strdup, ensures a NULL byte will be at the end of the buffer
 *
 * @param data : the source buffer to duplicate
 * @param len : the maximum size for the string (null byte is appended)
 */
char* proxenet_xstrdup(const char *data, size_t len)
{
	char* s;

	s = proxenet_xmalloc(len+1);
	if (!memcpy(s, data, len)) {
		xlog(LOG_CRITICAL, "proxenet_xstrdup() failed in memcpy: %s\n", strerror(errno));
		proxenet_xfree(s);
		return NULL;
	}

	return s;
}


/**
 * Wrapper for proxenet_xstrdup() using strlen() for determining argument length.
 * Should not be used if NULL bytes are in data.
 *
 * @param data : the source buffer to duplicate
 */
char* proxenet_xstrdup2(const char *data)
{
        return proxenet_xstrdup(data, strlen(data));
}


/**
 * Checks if the provided by is a valid plugin path i.e.:
 * - verifies that path is valid
 * - checks that a `autoload` sub-directory can be accessed
 *
 * @return true if all checks succeed, and autoload_path buffer is valid
 * @return false otherwise
 */
bool is_valid_plugin_path(char* plugin_path, char** plugins_path_ptr, char** autoload_path_ptr)
{
        char autoload_path[PATH_MAX] = {0,};

        /* check the plugins path */
        *plugins_path_ptr = realpath(plugin_path, NULL);
	if (*plugins_path_ptr == NULL){
		xlog(LOG_CRITICAL, "realpath('%s') failed: %s\n", plugin_path, strerror(errno));
		return false;
	}

        /* check the autoload path inside plugin path */
        strncpy(autoload_path, *plugins_path_ptr, PATH_MAX-1);
        strncat(autoload_path, CFG_DEFAULT_PLUGINS_AUTOLOAD_PATH, PATH_MAX-1);

        *autoload_path_ptr = realpath(autoload_path, NULL);
	if (*autoload_path_ptr == NULL){
		xlog(LOG_CRITICAL, "realpath('%s') failed: %s\n", autoload_path, strerror(errno));
                proxenet_xfree(*plugins_path_ptr);
                *plugins_path_ptr = NULL;
		return false;
	}

        return true;
}


/**
 * Check if the argument provided is a regular file
 *
 * @param path to the file to check
 * @return true is it's a regular, false otherwise
 */
bool is_file(char* path)
{
	struct stat buf;
	return (stat(path, &buf) || !S_ISREG(buf.st_mode)) ?  false : true;
}


/**
 * Check if the argument provided is a (regular) file and is readable by user calling
 *
 * @param path to the file to check
 * @return true is it's a regular and is readable, false otherwise
 */
bool is_readable_file(char* path)
{
	return is_file(path) && access(path, R_OK)==0;
}


/**
 * Prints a hexdump like version the buffer pointed by argument
 *
 * @param buf is a pointer to dump memory from
 * @param buflen is the number of bytes to dump
 */
void proxenet_hexdump(char *data, int size)
{
        int i;
        int j;
        char temp[8];
        char buffer[128];
        char *ascii;

        pthread_mutex_lock(&tty_mutex);

        printf("        +0          +4          +8          +c            0   4   8   c   \n");
        memset(buffer, 0, 128);
        ascii = buffer + 58;
        memset(buffer, ' ', 58 + 16);
        buffer[58 + 16] = '\n';
        buffer[58 + 17] = '\0';
        buffer[0] = '+';
        buffer[1] = '0';
        buffer[2] = '0';
        buffer[3] = '0';
        buffer[4] = '0';
        for (i = 0, j = 0; i < size; i++, j++) {
                if (j == 16) {
                        printf("%s", buffer);
                        memset(buffer, ' ', 58 + 16);
                        sprintf(temp, "+%04x", i);
                        memcpy(buffer, temp, 5);
                        j = 0;
                }

                sprintf(temp, "%02x", 0xff & data[i]);
                memcpy(buffer + 8 + (j * 3), temp, 2);
                if ((data[i] > 31) && (data[i] < 127))
                        ascii[j] = data[i];
                else
                        ascii[j] = '.';
        }

        if (j != 0)
                printf("%s", buffer);

        pthread_mutex_unlock(&tty_mutex);

        return;
}

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
#include <wordexp.h>

#include "core.h"
#include "utils.h"


static pthread_mutex_t tty_mutex;


/**
 *
 */
void _xlog(int type, const char* fmt, ...)
{
	va_list ap;
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);

	/* lock tty before printing */
        pthread_mutex_lock(&tty_mutex);

#ifdef DEBUG
        fprintf(cfg->logfile_fd,
                "%.4d/%.2d/%.2d ",
                tm.tm_year + 1900,
                tm.tm_mon + 1,
                tm.tm_mday);
#endif

        fprintf(cfg->logfile_fd,
                "%.2d:%.2d:%.2d-",
                tm.tm_hour, tm.tm_min,
                tm.tm_sec);


	switch (type) {
		case LOG_CRITICAL:
			if (cfg->use_color) fprintf(cfg->logfile_fd, DARK);
			fprintf(cfg->logfile_fd, "CRITICAL");
			break;

		case LOG_ERROR:
			if (cfg->use_color) fprintf(cfg->logfile_fd, RED);
			fprintf(cfg->logfile_fd, "ERROR");
			break;

		case LOG_WARNING:
			if (cfg->use_color) fprintf(cfg->logfile_fd, YELLOW);
			fprintf(cfg->logfile_fd, "WARNING");
			break;

		case LOG_DEBUG:
			if (cfg->use_color) fprintf(cfg->logfile_fd, BLUE);
			fprintf(cfg->logfile_fd, "DEBUG");
			break;

		case LOG_INFO:
		default:
			if (cfg->use_color) fprintf(cfg->logfile_fd, GREEN);
			fprintf(cfg->logfile_fd, "INFO");
			break;
	}


        if (cfg->use_color)
                fprintf(cfg->logfile_fd, NOCOLOR);

        fprintf(cfg->logfile_fd, "-");

#ifdef DEBUG
#if defined __LINUX__
	fprintf(cfg->logfile_fd, "tid-%lu ", pthread_self());
#elif defined __FREEBSD__ || defined __DARWIN__
        fprintf(cfg->logfile_fd, "tid-%p ", pthread_self());
#endif
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
		abort();
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
 * Remove tabs and spaces at the beginning of a string. The characters
 * are overwritten with NULL bytes.
 *
 * @param str : the buffer to strip from the left (beginning)
 */
void proxenet_lstrip(char* str)
{
        size_t i, j, d;
        size_t len = strlen(str);

        if (!len)
                return;

        for(i=0; i<=len-1 && (str[i]=='\t' || str[i]=='\n' || str[i]==' '); i++);
        if(i == len-1)
                return;

        d = len-i;
        for(j=0; j<d; j++, i++)
                str[j] = str[i];

        for(j=i-1; j<len; j++)
                str[j] = '\x00';

        return;
}


/**
 * Remove tabs and spaces at the end of the string by overwriting them with
 * NULL byte.
 *
 * @param str : the buffer to strip from the right (end)
 */
void proxenet_rstrip(char* str)
{
        size_t i, j;
        size_t len = strlen(str);

        if (!len)
                return;

        for(i=len-1; i && (str[i]=='\t' || str[i]=='\n' || str[i]==' '); i--);
        if(i == len-1)
                return;

        for(j=i; j<len; j++)
                str[j] = '\x00';

        return;
}


/**
 * Remove tabs and spaces at the beginning and the end of the string.
 *
 * @param str : the buffer to strip
 */
void proxenet_strip(char* str)
{
        proxenet_lstrip(str);
        proxenet_rstrip(str);
        return;
}


/**
 * Wrapper for snprintf(), returning the maximum number of bytes *actually* copied
 * in the destination buffer.
 *
 * @param str: the destination buffer
 * @param size: the maximum size to copy
 * @param format: the format string to apply
 */
inline int proxenet_xsnprintf(char *str, size_t size, const char *format, ...)
{
        int n;
        va_list ap;

        va_start(ap, format);
	n = vsnprintf(str, size, format, ap);
	va_end(ap);
        if (n < 0)
                return n;

        return MIN((unsigned int)n, (unsigned int)size);
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
        *plugins_path_ptr = expand_file_path(plugin_path);
	if (*plugins_path_ptr == NULL){
		return false;
	}

        /* check the autoload path inside plugin path */
        proxenet_xsnprintf(autoload_path, PATH_MAX-1, "%s/%s", *plugins_path_ptr, CFG_DEFAULT_PLUGINS_AUTOLOAD_PATHNAME);

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
 * Expand the file path, including with ~ or other bash characters.
 *
 * @return the absolute file path allocated in heap on success, NULL on failure
 */
char* expand_file_path(char* file_path)
{
        char *p, *p2;
        wordexp_t expanded_result;

        /* expand the path for bash character */
        if (wordexp(file_path, &expanded_result, 0)<0){
                xlog(LOG_CRITICAL, "wordexp('%s') failed: %s\n", file_path, strerror(errno));
                return NULL;
        }

        p = expanded_result.we_wordv[0];

        /* check the plugins path */
        p2 = realpath(p, NULL);
        wordfree(&expanded_result);

	if (p2 == NULL){
		xlog(LOG_CRITICAL, "realpath('%s') failed: %s\n", file_path, strerror(errno));
		return NULL;
	}

        return p2;
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
 * Check if the argument provided is a (regular) file and is writable by user calling
 *
 * @param path to the file to check
 * @return true is it's a regular and is writable, false otherwise
 */
bool is_writable_file(char* path)
{
	return is_file(path) && access(path, W_OK)==0;
}


/**
 * Check if the argument provided is a regular file
 *
 * @param path to the file to check
 * @return true is it's a regular, false otherwise
 */
bool is_dir(char* path)
{
	struct stat buf;
	return (stat(path, &buf) || !S_ISDIR(buf.st_mode)) ?  false : true;
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

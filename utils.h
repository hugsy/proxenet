#ifndef _UTILS_H
#define _UTILS_H

#ifndef SIZE_MAX
#define SIZE_MAX ~((size_t)1)
#endif

#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif

#define MAX(x,y) (((x) >= (y))?(x):(y)) 
#define MIN(x,y) (((x) <= (y))?(x):(y)) 

typedef enum {
	FALSE = 0,
	TRUE  = 1
} boolean;

enum log_level {
	LOG_DEBUG = 0,
	LOG_INFO,
	LOG_WARNING,
	LOG_ERROR,
	LOG_CRITICAL
};


#ifdef DEBUG
#define GEN_FMT "in %s (%s:%d) "
#define __xlog(t, ...) _xlog(t, __VA_ARGS__)
#define xlog(t, _f, ...) __xlog(t, GEN_FMT _f, __FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)

#else
#define xlog(t, ...) _xlog(t, __VA_ARGS__)
#endif

void _xlog(int type, const char* fmt, ...);
void* xmalloc(size_t size);
void xfree(void* ptr);
void xzero(void* buf, size_t buflen);
void* xrealloc(void* oldptr, size_t new_size);

#endif /* _UTILS_H */

#ifndef _PLUGINS_H
#define _PLUGINS_H

#include <pthread.h>

#include "utils.h"
#include "core.h"

typedef enum __supported_plugins_t {
#ifdef _PYTHON_PLUGIN
	_PYTHON_,
#endif

#ifdef _C_PLUGIN
	_C_,
#endif

#ifdef _RUBY_PLUGIN
	_RUBY_,
#endif

#ifdef _PERL_PLUGIN
	_PERL_,
#endif

#ifdef _LUA_PLUGIN
	_LUA_,
#endif

#ifdef _TCL_PLUGIN
	_TCL_,
#endif

  END_VALUE
} supported_plugins_t;


static const UNUSED char* supported_plugins_str[] = {
#ifdef _PYTHON_PLUGIN
	_PYTHON_VERSION_,
#endif

#ifdef _C_PLUGIN
	"C",
#endif

#ifdef _RUBY_PLUGIN
        _RUBY_VERSION_,
#endif

#ifdef _PERL_PLUGIN
	"Perl",
#endif

#ifdef _LUA_PLUGIN
	"Lua",
#endif

#ifdef _TCL_PLUGIN
	"Tcl",
#endif

  NULL
};


static const UNUSED char* plugins_extensions_str[] = {
#ifdef _PYTHON_PLUGIN
	".py",
#endif

#ifdef _C_PLUGIN
	".so",
#endif

#ifdef _RUBY_PLUGIN
	".rb",
#endif

#ifdef _PERL_PLUGIN
	".pl",
#endif

#ifdef _LUA_PLUGIN
	".lua",
#endif

#ifdef _LUA_PLUGIN
	".tcl",
#endif

  NULL
};

#define MAX_VMS 6

typedef struct _http_request_fields
{
	  char* method;
	  char* proto;
	  bool is_ssl;
	  char* hostname;
	  unsigned short port;
	  char* uri;
	  char* version;
} http_request_t ;

typedef enum {
	REQUEST	 = 0,
	RESPONSE = 1
} req_t;

typedef struct _request_type {
		long id;
		req_t type;
		char* data;
		size_t size;
		http_request_t http_infos;
} request_t;

typedef struct _interpreter_type {
		unsigned short id;
		supported_plugins_t type;
		pthread_mutex_t mutex;
		void *vm;
		bool ready;
} interpreter_t;

interpreter_t vms[MAX_VMS];


typedef struct _plugin_type {
		unsigned short id;
		char* filename;
		char* name;
		supported_plugins_t type;
		unsigned short priority;
		struct _plugin_type* next;
		proxenet_state state;

		interpreter_t *interpreter;
		void *pre_function;
		void *post_function;

} plugin_t;

#include "http.h"

#define PROXENET_ABSOLUTE_PLUGIN_PATH(f, p)                             \
        {                                                               \
                size_t len;                                             \
                len = strlen(cfg->plugins_path) + 1 + strlen(f) + 1;    \
                p = alloca(len + 1);                                    \
                proxenet_xzero(p, len + 1);                             \
                snprintf(p, len, "%s/%s", cfg->plugins_path, f);        \
        }

int		proxenet_add_new_plugins(char*, char*);
unsigned int 	proxenet_plugin_list_size();
void		proxenet_remove_plugin(plugin_t*);
void 		proxenet_remove_all_plugins();
void 		proxenet_print_plugins_list();
int		count_plugins_by_type(int);
char*		get_plugin_path(char*);
int		count_initialized_plugins_by_type(int);


#endif /* _PLUGINS_H */

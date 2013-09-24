#ifndef _PLUGINS_H
#define _PLUGINS_H

#include <pthread.h>

#include "utils.h"


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

#ifdef HAVE_LIBLUA5_2
	_LUA_,
#endif
	
  END_VALUE
} supported_plugins_t;


const static UNUSED char* supported_plugins_str[] = {
#ifdef _PYTHON_PLUGIN
	#if defined _PYTHON2_
	"Python2",
	#elif defined _PYTHON3_
	"Python3",
	#else
	"Unknown Python version (incorrect build?)",
	#endif
#endif

#ifdef _C_PLUGIN
	"C",
#endif
	
#ifdef _RUBY_PLUGIN
	#if defined _RUBY18_
	"Ruby 1.8",
	#elif defined _RUBY19_
	"Ruby 1.9",
	#else
	"Unknown Ruby version (incorrect build?)",
	#endif	
#endif

#ifdef _PERL_PLUGIN
	"Perl",
#endif

#ifdef HAVE_LIBLUA5_2
	"Lua",
#endif
	
  NULL
};


const static UNUSED char* plugins_extensions_str[] = {
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

#ifdef HAVE_LIBLUA5_2
	".lua",
#endif
	
  NULL
};


enum {
	REQUEST	 = 0,
	RESPONSE = 1
};

#define MAX_VMS 5
	
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
		unsigned char state;

		interpreter_t *interpreter;
		void *pre_function;
		void *post_function;
		
} plugin_t;


int	proxenet_create_list_plugins(char*);
int 	proxenet_plugin_list_size();
void	proxenet_free_plugin(plugin_t*);
void 	proxenet_delete_list_plugins();
void 	proxenet_print_plugins_list();
int	count_plugins_by_type(int);
char*	get_plugin_path(char*);
int	count_initialized_plugins_by_type(int);
char*	proxenet_build_plugins_list();

#endif /* _PLUGINS_H */

#ifdef _RUBY_PLUGIN

#include <ruby.h>

#include "plugin.h"

#ifdef _RUBY_VERSION_1_9
extern VALUE rb_vm_top_self(void);
#else
extern VALUE ruby_top_self;
#endif

int 	proxenet_ruby_initialize_vm(plugin_t*);
int	proxenet_ruby_destroy_vm(plugin_t*);
int 	proxenet_ruby_initialize_function(plugin_t*, int);
char* 	proxenet_ruby_execute_function(interpreter_t*, ID, long, char*);
char* 	proxenet_ruby_plugin(plugin_t*, long, char*, int);

#endif /* _RUBY_PLUGIN */

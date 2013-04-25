#ifdef _RUBY_PLUGIN

#include "plugin.h"

extern VALUE rb_vm_top_self(void);

int 	proxenet_ruby_initialize_vm(plugin_t*);
int	proxenet_ruby_destroy_vm(plugin_t*);
int 	proxenet_ruby_initialize_function(plugin_t*, int);
char* 	proxenet_ruby_execute_function(PyObject*, long, char*);
char* 	proxenet_ruby_plugin(plugin_t*, long, char*, int);

#endif /* _RUBY_PLUGIN */

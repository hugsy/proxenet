#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _RUBY_PLUGIN

#include <ruby.h>

#include "plugin.h"

#if _RUBY_MINOR_ == 9
extern VALUE rb_vm_top_self(void);
#else
extern VALUE ruby_top_self;
#endif

int 	proxenet_ruby_initialize_vm(plugin_t*);
int	proxenet_ruby_destroy_plugin(plugin_t*);
int	proxenet_ruby_destroy_vm(interpreter_t*);
int 	proxenet_ruby_load_file(plugin_t*);
char* 	proxenet_ruby_plugin(plugin_t*, request_t*);

#endif /* _RUBY_PLUGIN */

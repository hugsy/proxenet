#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _C_PLUGIN

#include "plugin.h"

int 	proxenet_c_initialize_vm(plugin_t*);
int 	proxenet_c_destroy_plugin(plugin_t*);
int 	proxenet_c_destroy_vm(interpreter_t*);
int 	proxenet_c_initialize_function(plugin_t*, req_t);
char* 	proxenet_c_plugin(plugin_t*, request_t*);

#endif

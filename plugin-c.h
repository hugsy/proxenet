#ifdef _C_PLUGIN

#include "plugin.h"

int 	proxenet_c_initialize_vm(plugin_t*);
int 	proxenet_c_destroy_vm(plugin_t*);
int 	proxenet_c_initialize_function(plugin_t*, int);
char* 	proxenet_c_plugin(plugin_t*, long, char*, size_t*, int);

#endif

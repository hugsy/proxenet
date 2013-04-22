#ifdef _PYTHON_PLUGIN

#include "plugin.h"

int 	proxenet_python_add_plugins_path(char*);
void 	proxenet_python_initialize_vm(plugin_t*);
void 	proxenet_python_destroy_vm(plugin_t*);
char* 	proxenet_python_plugin(plugin_t*, char*, const char*);


#endif

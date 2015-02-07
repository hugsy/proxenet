#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _PYTHON_PLUGIN

#include <Python.h>
#include "plugin.h"

int     proxenet_python_initialize_vm(plugin_t*);
int     proxenet_python_destroy_vm(plugin_t*);
int     proxenet_python_initialize_function(plugin_t*, req_t);
char*   proxenet_python_plugin(plugin_t*, request_t*);

#endif

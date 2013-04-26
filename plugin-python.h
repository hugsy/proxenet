#ifdef _PYTHON_PLUGIN

#include <Python.h>

#include "plugin.h"

int 	proxenet_python_add_plugins_path(char*);
int 	proxenet_python_initialize_vm(plugin_t*);
int 	proxenet_python_destroy_vm(plugin_t*);
int	proxenet_python_initialize_function(plugin_t*, char);
char*   proxenet_python_execute_function(PyObject*, long, char*);
char* 	proxenet_python_plugin(plugin_t*, long, char*, int);

#endif

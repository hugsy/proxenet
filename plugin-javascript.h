#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _JAVASCRIPT_PLUGIN

#include "plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

int proxenet_javascript_initialize_vm(plugin_t* plugin);
int proxenet_javascript_destroy_vm(interpreter_t* interpreter);
int proxenet_javascript_load_file(plugin_t* plugin);
int proxenet_javascript_destroy_plugin(plugin_t* plugin);
char* proxenet_javascript_plugin(plugin_t *plugin, request_t *request);

#ifdef __cplusplus
}
#endif

#endif /* _JAVASCRIPT_PLUGIN */

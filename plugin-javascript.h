#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _JAVASCRIPT_PLUGIN

#include "plugin.h"
#include <v8.h>

using namespace v8;

typedef struct {
                v8::HandleScope handle_scope;
                v8::Persistent<v8::Context> context;
                v8::Handle<v8::ObjectTemplate> global;
                v8::Handle<v8::String> source;
                Handle<Script> script;
} proxenet_js_t;


int proxenet_javascript_initialize_vm(plugin_t* plugin);
int proxenet_javascript_destroy_vm(interpreter_t* interpreter);
int proxenet_javascript_initialize_plugin(plugin_t* plugin);
int proxenet_javascript_destroy_plugin(plugin_t* plugin);
char* proxenet_javascript_plugin(plugin_t *plugin, request_t *request);

#endif /* _JAVASCRIPT_PLUGIN */

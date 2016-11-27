#undef _

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _JAVASCRIPT_PLUGIN

/*******************************************************************************
 *
 * JavaScript plugin (using V8 engine - https://v8docs.nodesource.com)
 *
 */

#include <dlfcn.h>
#include <string.h>
#include <cstdio>

#ifdef __LINUX__
#include <alloca.h>
#endif

extern "C"
{
#include "core.h"
#include "utils.h"
#include "main.h"
#include "plugin.h"
}

#include "plugin-javascript.h"
#include <v8.h>

using namespace v8;

#define xlog_js(t, ...) xlog(t, "[" _JAVASCRIPT_VERSION_ "] " __VA_ARGS__)


/**
 *
 */
static void proxenet_javascript_print_exception(v8::TryCatch *trycatch)
{
    Local<Value> exception = trycatch->Exception();
    String::Utf8Value str_exception(exception);
    printf("Exception raised: %s\n", *str_exception);
}


/**
 *
 */
int proxenet_javascript_initialize_vm(plugin_t* plugin)
{
	proxenet_js_t* vm;

        vm = (proxenet_js_t*)proxenet_xmalloc(sizeof(proxenet_js_t));
        vm->global = ObjectTemplate::New();
        vm->context = Context::New(NULL, vm->global);

	plugin->interpreter->vm = (void*)vm;
        plugin->interpreter->ready = true;
	return 0;
}


/**
 *
 */
int proxenet_javascript_destroy_plugin(plugin_t* plugin)
{
        proxenet_plugin_set_state(plugin, INACTIVE);
        plugin->onload_function = NULL;
        plugin->pre_function = NULL;
        plugin->post_function = NULL;
        plugin->onleave_function = NULL;
        return 0;
}


/**
 *
 */
int proxenet_javascript_destroy_vm(interpreter_t* interpreter)
{
        proxenet_js_t* vm = (proxenet_js_t*)interpreter->vm;
        vm->context.Dispose();
        proxenet_xfree(vm);

        interpreter->vm = NULL;
        interpreter->ready = false;
        interpreter = NULL;
        return 0;
}


/**
 *
 */
static bool is_valid_function(interpreter_t* interpreter, const char *function)
{
        proxenet_js_t* vm = (proxenet_js_t*)interpreter->vm;
        Context::Scope context_scope(vm->context);
        Handle<Object> global = vm->context->Global();
        Handle<Value> value = global->Get(String::New(function));
        return value->IsFunction();
}


/**
 *
 */
int proxenet_javascript_initialize_plugin(plugin_t* plugin)
{
        interpreter_t* interpreter;
	proxenet_js_t *vm;
        v8::TryCatch trycatch;
        const char* function_names[5] = { CFG_REQUEST_PLUGIN_FUNCTION,
                                          CFG_RESPONSE_PLUGIN_FUNCTION,
                                          CFG_ONLOAD_PLUGIN_FUNCTION,
                                          CFG_ONLEAVE_PLUGIN_FUNCTION,
                                          NULL
        };
        const char **n;

        if (plugin->interpreter==NULL || plugin->interpreter->ready==false){
                //xlog_js(LOG_ERROR, "%s\n", "not ready");
                return -1;
        }

        interpreter = (interpreter_t*)plugin->interpreter;
        vm = (proxenet_js_t*)interpreter->vm;

        // switch to plugin context
        Context::Scope context_scope(vm->context);
        vm->source = String::New(plugin->fullpath);
        vm->script = Script::Compile(vm->source);

        if (vm->script.IsEmpty()){
                proxenet_javascript_print_exception(&trycatch);
                return -1;
        }

        Local<Value> value = vm->script->Run();
        if (value.IsEmpty()) {
                proxenet_javascript_print_exception(&trycatch);
                return -1;
        }

        for(n=function_names; *n; n++){
                if(!is_valid_function(interpreter, *n)){
                        //xlog_js("'%s' is not a valid function\n", *n);
                        return -1;
                }
        }

        return 0;
}


/**
 *
 */
char* proxenet_javascript_plugin(plugin_t *plugin, request_t *request)
{
	return NULL;
}

#endif

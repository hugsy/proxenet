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

extern "C"
{
#include "core.h"
#include "utils.h"
#include "main.h"
#include "plugin.h"
}

#include "plugin-javascript.h"
#include <v8.h>

#define xlog_js(t, ...) xlog(t, "[" _JAVASCRIPT_VERSION_ "] " __VA_ARGS__)

using namespace v8;

v8::HandleScope handle_scope;

typedef struct {
                v8::Persistent<v8::Context> context;
                // v8::Handle<v8::ObjectTemplate> global;
                v8::Handle<Script> script;
} proxenet_js_t;


/**
 *
 */
static void proxenet_javascript_print_exception(v8::TryCatch *trycatch)
{
        Local<Value> exception = trycatch->Exception();
        String::Utf8Value str_exception(exception);
        xlog_js(LOG_ERROR, "Exception raised: %s\n", *str_exception);
}


/**
 *
 */
int proxenet_javascript_initialize_vm(plugin_t* plugin)
{
        proxenet_js_t* vm;

        if(!plugin->interpreter->ready){
                V8::Initialize();
        }

        vm = (proxenet_js_t*)proxenet_xmalloc(sizeof(proxenet_js_t));
        // vm->global = ObjectTemplate::New();
        vm->context = Context::New();

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
        Handle<v8::Object> global = vm->context->Global();
        Handle<v8::Value> value = global->Get(String::New(function));
        vm->context.Dispose();
        return value->IsFunction();
}


/**
 *
 */
int proxenet_javascript_load_file(plugin_t* plugin)
{
        interpreter_t* interpreter;
        proxenet_js_t *vm;
        char* js_code;
        const char* function_names[5] = { CFG_REQUEST_PLUGIN_FUNCTION,
                                          CFG_RESPONSE_PLUGIN_FUNCTION,
                                          CFG_ONLOAD_PLUGIN_FUNCTION,
                                          CFG_ONLEAVE_PLUGIN_FUNCTION,
                                          NULL
        };
        const char **n;

        v8::TryCatch trycatch;
        v8::Handle<v8::String> source;

        if (!plugin->interpreter || !plugin->interpreter->ready){
                xlog_js(LOG_ERROR, "%s\n", "not ready");
                return -1;
        }

        interpreter = (interpreter_t*)plugin->interpreter;
        vm = (proxenet_js_t*)interpreter->vm;

        /* read the code of the plugin */
        js_code = get_file_content(plugin->fullpath);
        if (!js_code)
                return -1;

        /* switch to plugin context */
        Context::Scope context_scope(vm->context);

        /* JS compile the code */
        source = String::New(js_code);
        vm->script = Script::Compile(source);
#ifdef DEBUG
        xlog_js(LOG_DEBUG, "JavaScript code of '%s' compiled\n", plugin->fullpath);
#endif

        proxenet_xfree(js_code);

        /* check if there is data in the file */
        if (vm->script.IsEmpty()){
                xlog_js(LOG_WARNING, "'%s' is empty, freeing...\n", plugin->fullpath);
                return -1;
        }

        Local<Value> value = vm->script->Run();
        if (value.IsEmpty()) {
                proxenet_javascript_print_exception(&trycatch);
                return -1;
        }

        for(n=function_names; *n; n++){
                if(!is_valid_function(interpreter, *n)){
                        xlog_js(LOG_ERROR, "'%s' is not a valid function\n", *n);
                        return -1;
                }
        }

        return 0;
}


/**
 *
 */
static void proxenet_javascript_lock_vm(interpreter_t *interpreter)
{
        pthread_mutex_lock(&interpreter->mutex);

}


/**
 *
 */
static void proxenet_javascript_unlock_vm(interpreter_t *interpreter)
{
        pthread_mutex_unlock(&interpreter->mutex);
}


/**
 *
 */
char* proxenet_javascript_plugin(plugin_t *plugin, request_t *request)
{
        interpreter_t *interpreter;
        proxenet_js_t *vm;
        Handle<v8::Object> global;
        Handle<v8::Value> value;
        Handle<v8::Function> func;
        Handle<Value> args[3];
        Handle<Value> js_result;

        interpreter = plugin->interpreter;
        vm = (proxenet_js_t*)interpreter->vm;

        proxenet_javascript_lock_vm(interpreter);
        global = vm->context->Global();

        switch(request->type){
                case REQUEST:
                        value = global->Get(String::New(CFG_REQUEST_PLUGIN_FUNCTION));
                        break;
                case RESPONSE:
                        value = global->Get(String::New(CFG_RESPONSE_PLUGIN_FUNCTION));
                        break;
                default:
                        xlog(LOG_CRITICAL, "%s\n", "Should never be here");
                        abort();
        }

        /* the check is `func` exists was done in proxenet_javascript_load_file() */
        func = v8::Handle<v8::Function>::Cast(value);

        /* prepare the arguments as v8 structs, and call the function */
        args[0] = v8::Int32::New(request->id);
        args[1] = v8::String::New(request->data);
        args[2] = v8::String::New(request->http_infos.uri);
        js_result = func->Call(global, 3, args);

        proxenet_javascript_unlock_vm(interpreter);

        /* convert the result to char */
        String::AsciiValue result(js_result);

        return *result;
}

#endif

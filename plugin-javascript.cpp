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
                v8::Isolate *env;
                v8::Persistent<v8::Context> context;
                v8::HandleScope handle_scope;
                v8::Handle<Script> script;
                v8::Handle<v8::ObjectTemplate> global;
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

#ifdef DEBUG
        xlog_js(LOG_DEBUG, "Loading JavaScript VM (v8: %s)\n", _JAVASCRIPT_VERSION_);
#endif

        if(!plugin->interpreter->ready){
                V8::Initialize();
        }

        vm = (proxenet_js_t*)proxenet_xmalloc(sizeof(proxenet_js_t));
        vm->global = ObjectTemplate::New();
        vm->context = Context::New();

        plugin->interpreter->vm = (void*)vm;
        plugin->interpreter->ready = true;

        if (cfg->verbose)
                xlog(LOG_INFO, "JavaScript VM (using v8: %s) successfully loaded\n", _JAVASCRIPT_VERSION_);
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
        if (cfg->verbose)
                xlog(LOG_INFO, "Successfully disabled\n", plugin->name);
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

        if (cfg->verbose)
                xlog(LOG_INFO, "JavaScript VM (using v8: %s) unloaded\n", _JAVASCRIPT_VERSION_);
        return 0;
}


/**
 *
 */
static inline bool is_valid_function(interpreter_t* interpreter, char* plugin_name, const char *function)
{
#ifdef DEBUG
                xlog_js(LOG_DEBUG, "Checking if '%s.%s' is a valid method\n", plugin_name, function);
#endif
        proxenet_js_t* vm = (proxenet_js_t*)interpreter->vm;
        Handle<v8::Object> global = vm->context->Global();

        Handle<Function> classptr = v8::Handle<v8::Function>::Cast(global->Get(String::New(plugin_name)));
        if(!classptr->IsFunction()){
                xlog_js(LOG_ERROR, "'%s' is not a valid class\n", plugin_name);
                return false;
        }

        Handle<Object> object = classptr->NewInstance(0, NULL);
        Handle<v8::Value> value = object->Get(String::New(function));
        if (value->IsFunction())
                return true;

        xlog_js(LOG_ERROR, "'%s.%s' is not a valid function\n", plugin_name, function);
        return false;
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
                if(!is_valid_function(interpreter, plugin->name, *n)){
                        return -1;
                }
        }

        if(cfg->verbose)
                xlog_js(LOG_INFO, "Plugin '%s' successfully loaded\n", plugin->name);

        vm->context.Dispose();
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

// todo implem on_load() and on_leave() calls

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
        Context::Scope context_scope(vm->context);
        global = vm->context->Global();

        /* get a handle to class "plugin_name" */
        Handle<Function> classptr = v8::Handle<v8::Function>::Cast(global->Get(String::New(plugin->name)));

        /* instanciate it */
        Handle<Object> object = classptr->NewInstance(0, NULL);

        /* only now can we call the proxenet hooks */
        switch(request->type){
                case REQUEST:
                        value = object->Get(String::New(CFG_REQUEST_PLUGIN_FUNCTION));
                        break;
                case RESPONSE:
                        value = object->Get(String::New(CFG_RESPONSE_PLUGIN_FUNCTION));
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

        vm->context.Dispose();
        proxenet_javascript_unlock_vm(interpreter);

        /* convert the result to char */
        String::AsciiValue result(js_result);

        return *result;
}

#endif

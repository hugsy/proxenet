#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _RUBY_PLUGIN

/*******************************************************************************
 *
 * Ruby plugin
 *
 */


#include <pthread.h>
#include <string.h>
#include <ctype.h>
#include <ruby.h>
#include <ruby/thread.h>

#include "core.h"
#include "plugin.h"
#include "plugin-ruby.h"
#include "utils.h"
#include "main.h"

struct proxenet_ruby_args {
        VALUE rVM;
        ID rFunc;
        VALUE rArgs[3];
};


#define xlog_ruby(t, ...) xlog(t, "["_RUBY_VERSION_"] " __VA_ARGS__)


/**
 * proxenet wrapper for Ruby rb_intern()
 */
static VALUE rb_intern_wrap(VALUE arg)
{
        return rb_intern((char*)arg);
}


/**
 * proxenet wrapper for Ruby rb_const_get()
 */
static VALUE rb_const_get_wrap(VALUE arg)
{
        return rb_const_get(rb_cObject, arg);
}


/**
 * Print message from last exception triggered by ruby
 */
static void proxenet_ruby_print_last_exception()
{
        VALUE rException, rExceptStr;

        rException = rb_errinfo();         /* get last exception */
        rb_set_errinfo(Qnil);              /* clear last exception */
        rExceptStr = rb_funcall(rException, rb_intern("to_s"), 0, Qnil);
        xlog_ruby(LOG_ERROR, "Exception: %s\n", StringValuePtr(rExceptStr));
        return;
}


/**
 *
 */
static VALUE proxenet_ruby_get_module_reference(char* name)
{
        VALUE rModule, rPlugin;
        int err;
        char *plugin_name;

        plugin_name = (isdigit(name[0])) ? name + 1 : name;

        rPlugin = rb_protect( rb_intern_wrap, (VALUE)name, &err);
        if (err){
                xlog_ruby(LOG_ERROR, "%s(%s) failed miserably\n", "rb_intern", plugin_name);
                proxenet_ruby_print_last_exception();
                return 0;
        }

        rModule = rb_protect( rb_const_get_wrap, rPlugin, &err);
        if (err){
                xlog_ruby(LOG_ERROR, "%s(%s) failed miserably\n", "rb_const_get", plugin_name);
                proxenet_ruby_print_last_exception();
                return 0;
        }

        return rModule;
}


/**
 * Initialize Ruby VM.
 */
int proxenet_ruby_initialize_vm(plugin_t* plugin)
{
        static char* rArgs[2] = { "ruby", "/dev/null" };
        interpreter_t *interpreter;
        VALUE rRet;

        interpreter = plugin->interpreter;

        /* checks */
        if (interpreter->ready)
                return 0;

#ifdef DEBUG
        xlog_ruby(LOG_DEBUG, "Initializing Ruby VM version %s\n", _RUBY_VERSION_);
#endif

        /* init vm */
        ruby_init();

        /*
         * The hack of calling ruby_process_options() with /dev/null allows to simply (in one call)
         * init all the structures and encoding params by the vm
         * Details: http://rxr.whitequark.org/mri/source/ruby.c#1915
         */
        rRet = (VALUE)ruby_process_options(2, rArgs);

        if (rRet == Qnil) {
                xlog_ruby(LOG_ERROR, "Error on ruby_process_options(): %#x\n", rRet);
                proxenet_ruby_print_last_exception();
                return -1;
        }

        interpreter->vm = (void*) rb_mKernel;
        interpreter->ready = true;

        return 0;
}


/**
 * Clean up Ruby VM context and disable the interpreter for proxenet.
 */
int proxenet_ruby_destroy_vm(interpreter_t* interpreter)
{
	ruby_cleanup(0);

        interpreter->ready = false;
        interpreter->vm = NULL;

	return 0;
}


/**
 * Initialize Ruby plugin
 */
static int proxenet_ruby_initialize_function(plugin_t* plugin, req_t type)
{
        int err;

	/* get function ID */
        switch(type) {
                case REQUEST:
                        if (plugin->pre_function) {
                                return 0;
                        }

                        plugin->pre_function  = (void*)rb_protect(rb_intern_wrap, (VALUE)CFG_REQUEST_PLUGIN_FUNCTION, &err);
                        if (err){
                                xlog_ruby(LOG_ERROR, "Failed to get '%s'\n", CFG_REQUEST_PLUGIN_FUNCTION);
                                proxenet_ruby_print_last_exception();
                                return -1;
                        }

                        if (plugin->pre_function) {
#ifdef DEBUG
                                xlog_ruby(LOG_DEBUG, "Loaded %s:%s\n", plugin->filename, CFG_REQUEST_PLUGIN_FUNCTION);
#endif
                                return 0;
                        }
                        break;

                case RESPONSE:
                        if (plugin->post_function) {
                                return 0;
                        }

                        plugin->post_function = (void*)rb_protect(rb_intern_wrap, (VALUE)CFG_RESPONSE_PLUGIN_FUNCTION, &err);
                        if (err){
                                xlog_ruby(LOG_ERROR, "Failed to get '%s'\n", CFG_RESPONSE_PLUGIN_FUNCTION);
                                proxenet_ruby_print_last_exception();
                                return -1;
                        }

                        if (plugin->post_function) {
#ifdef DEBUG
                                xlog_ruby(LOG_DEBUG, "Loaded %s:%s\n", plugin->filename, CFG_RESPONSE_PLUGIN_FUNCTION);
#endif
                                return 0;
                        }
                        break;

                case ONLOAD:
                        plugin->onload_function = (void*)rb_protect(rb_intern_wrap, (VALUE)CFG_ONLOAD_PLUGIN_FUNCTION, &err);
                        if (err){
                                xlog_ruby(LOG_WARNING, "Failed to get '%s'\n", CFG_ONLOAD_PLUGIN_FUNCTION);
                        }
                        return 0;

                        break;


                case ONLEAVE:
                        plugin->onleave_function = (void*)rb_protect(rb_intern_wrap, (VALUE)CFG_ONLEAVE_PLUGIN_FUNCTION, &err);
                        if (err){
                                xlog_ruby(LOG_WARNING, "Failed to get '%s'\n", CFG_ONLEAVE_PLUGIN_FUNCTION);
                        }
                        return 0;

                        break;

                default:
                        xlog_ruby(LOG_CRITICAL, "%s\n", "Should never be here, autokill !");
                        abort();
                        break;
        }

        xlog_ruby(LOG_ERROR, "%s\n", "Failed to find function");

        return -1;
}


/**
 * This function will safely be executed (gvl acquired)
 * this should not create a blocking/bottleneck situation since the access to rb_mKernel is
 * mutex-protected by proxenet before.
 * This function assumes that exactly 3 arguments are expected: the id, the buffer, the size
 * of this buffer.
 *
 * @return the result of rb_funcall2()
 */
static VALUE proxenet_ruby_safe_plugin_funcall2(VALUE arg)
{
        struct proxenet_ruby_args* args = (struct proxenet_ruby_args*) arg;
        return rb_funcall2(args->rVM, args->rFunc, 3, args->rArgs);
}


/**
 *
 */
static VALUE proxenet_ruby_safe_funcall2(VALUE arg)
{
        struct proxenet_ruby_args* args = (struct proxenet_ruby_args*) arg;
        return rb_funcall2(args->rVM, args->rFunc, 0, NULL);
}


/**
 * Safely load a Ruby script in the VM
 *
 * @return 0 upon success, -1 otherwise
 */
int proxenet_ruby_load_file(plugin_t* plugin)
{
        char* pathname;
        int res = 0;
        VALUE rRet;
        struct proxenet_ruby_args args;

        if (!plugin->interpreter || !plugin->interpreter->ready){
                xlog_ruby(LOG_ERROR, "Interpreter '%s' is not ready\n", _RUBY_VERSION_);
                return -1;
        }

        if(plugin->state != INACTIVE){
#ifdef DEBUG
                if(cfg->verbose > 2)
                        xlog_ruby(LOG_DEBUG, "Plugin '%s' is already loaded. Skipping...\n", plugin->name);
#endif
                return 0;
        }

        pathname = plugin->fullpath;

        rb_load_protect(rb_str_new2(pathname), false, &res);
        if (res != 0) {
                xlog_ruby(LOG_ERROR, "Error %d when load file '%s'\n", res, pathname);
                proxenet_ruby_print_last_exception();
                return -1;
        }

        if (cfg->verbose)
                xlog_ruby(LOG_INFO, "File '%s' is loaded\n", pathname);

        /* Storing function pointers in structure */
        if (proxenet_ruby_initialize_function(plugin, REQUEST) < 0) {
                proxenet_plugin_set_state(plugin, INACTIVE);
                xlog_ruby(LOG_ERROR, "Failed to init %s in %s\n", CFG_REQUEST_PLUGIN_FUNCTION, plugin->name);
                return -1;
        }

        if (proxenet_ruby_initialize_function(plugin, RESPONSE) < 0) {
                proxenet_plugin_set_state(plugin, INACTIVE);
                xlog_ruby(LOG_ERROR, "Failed to init %s in %s\n", CFG_RESPONSE_PLUGIN_FUNCTION, plugin->name);
                return -1;
        }

        if (proxenet_ruby_initialize_function(plugin, ONLOAD) < 0) {
                xlog_ruby(LOG_ERROR, "An error occured during initialization of %s in %s\n", CFG_ONLOAD_PLUGIN_FUNCTION, plugin->name);
        }

        if (proxenet_ruby_initialize_function(plugin, ONLEAVE) < 0) {
                xlog_ruby(LOG_ERROR, "An error occured during initialization of %s in %s\n", CFG_ONLEAVE_PLUGIN_FUNCTION, plugin->name);
        }

        /* Calling the on_load() function */
        if(plugin->onload_function){
                args.rVM = (VALUE)proxenet_ruby_get_module_reference(plugin->name);
                args.rFunc = (ID)plugin->onload_function;
                rRet = rb_protect( proxenet_ruby_safe_funcall2, (VALUE)&args, &res );
                if (res || !rRet){
                        proxenet_ruby_print_last_exception();
                }
        }

        return 0;
}


/**
 * Cleanly disable the plugin, call the onleave() function (if any) and unallocate the plugin
 * structures.
 */
int proxenet_ruby_destroy_plugin(plugin_t* plugin)
{
        int res;
        VALUE rRet;
        struct proxenet_ruby_args args;

        proxenet_plugin_set_state(plugin, INACTIVE);

        /* Calling the on_leave() function */
        if(plugin->onleave_function){
                args.rVM = (VALUE)proxenet_ruby_get_module_reference(plugin->name);
                args.rFunc = (ID)plugin->onleave_function;
                rRet = rb_protect( proxenet_ruby_safe_funcall2, (VALUE)&args, &res );
                if (res || !rRet){
                        proxenet_ruby_print_last_exception();
                }
        }

        plugin->pre_function = NULL;
        plugin->post_function = NULL;
        plugin->onload_function = NULL;
        plugin->onleave_function = NULL;

        return 0;
}



/**
 *
 */
static char* proxenet_ruby_execute_function(VALUE module, ID rFunc, request_t* request)
{
	char *buf, *data;
	int buflen, i, state;
	VALUE rRet;
	char *uri;
        struct proxenet_ruby_args args;

	uri = request->http_infos.uri;
	if (!uri)
		return NULL;

	/* build args */
	args.rVM = module;

        args.rFunc = rFunc;

	args.rArgs[0] = INT2NUM(request->id);
	args.rArgs[1] = rb_str_new(request->data, request->size);
	args.rArgs[2] = rb_str_new2(uri);

	for(i=0; i<3; i++) {
	  rb_gc_register_address(&args.rArgs[i]);
	}

	/* safe function call */
	rRet = rb_protect(proxenet_ruby_safe_plugin_funcall2, (VALUE)&args, &state);

	if (state){
		proxenet_ruby_print_last_exception();
		data = NULL;
		goto call_end;
	}

	if (!rRet) {
		xlog_ruby(LOG_ERROR, "%s\n", "funcall2() failed");
		data = NULL;
		goto call_end;
	}

	rb_check_type(rRet, T_STRING);


	/* copy result to exploitable buffer */
	buf = RSTRING_PTR(rRet);
	buflen = RSTRING_LEN(rRet);

	data = proxenet_xmalloc(buflen + 1);
	data = memcpy(data, buf, buflen);

	request->data = data;
	request->size = buflen;

call_end:

	for(i=0; i<3; i++) {
		rb_gc_unregister_address(&args.rArgs[i]);
        }

	return data;
}


/**
 * Lock Ruby VM
 */
static inline void proxenet_ruby_lock_vm(interpreter_t *interpreter)
{
	pthread_mutex_lock(&interpreter->mutex);
}


/**
 * Unlock Ruby VM
 */
static inline void proxenet_ruby_unlock_vm(interpreter_t *interpreter)
{
	pthread_mutex_unlock(&interpreter->mutex);
}


/**
 *
 */
char* proxenet_ruby_plugin(plugin_t* plugin, request_t* request)
{
	char *buf = NULL;
	interpreter_t *interpreter = plugin->interpreter;;
	VALUE module;
	ID rFunc;

        module = proxenet_ruby_get_module_reference(plugin->name);
        if(!module)
                return buf;

	switch(request->type){
                case REQUEST:
        		rFunc = (ID) plugin->pre_function;
                        break;
                case RESPONSE:
                        rFunc = (ID) plugin->post_function;
                        break;
                default:
                        abort();
        }

        proxenet_ruby_lock_vm(interpreter);
	buf = proxenet_ruby_execute_function(module, rFunc, request);
	proxenet_ruby_unlock_vm(interpreter);

	return buf;
}

#endif /* _RUBY_PLUGIN */

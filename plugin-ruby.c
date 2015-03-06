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
#include <alloca.h>
#include <string.h>
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


/**
 * Ruby require callback
 *
 */
VALUE proxenet_ruby_require_cb(VALUE arg)
{
	return rb_require((char *)arg);
}


/**
 *
 */
int proxenet_ruby_load_file(plugin_t* plugin)
{
	char* filename;
	char* pathname;
	int res = 0;

        if(plugin->state != INACTIVE){
#ifdef DEBUG
                if(cfg->verbose > 2)
                        xlog(LOG_DEBUG, "Plugin '%s' is already loaded. Skipping...\n", plugin->name);
#endif
                return 0;
        }

	filename = plugin->filename;
        pathname = plugin->fullpath;

	rb_load_protect(rb_str_new_cstr(pathname), 0, &res);
	if (res != 0) {
		xlog(LOG_ERROR, "[Ruby] Error %d when load file '%s'\n", res, pathname);
		return -1;
	}

#ifdef DEBUG
	xlog(LOG_DEBUG, "%s\n", pathname);
#endif
	return 0;
}


/**
 *
 */
int proxenet_ruby_initialize_vm(plugin_t* plugin)
{
	interpreter_t *interpreter;

	interpreter = plugin->interpreter;

	/* checks */
	if (interpreter->ready)
		return 0;

#ifdef DEBUG
	xlog(LOG_DEBUG, "Initializing Ruby VM version %s\n", _RUBY_VERSION_);
#endif

	/* init vm */
	ruby_init();

	interpreter->vm = (void*) rb_mKernel;

        ruby_script(PROGNAME);

	ruby_init_loadpath();

	interpreter->ready = true;

	return 0;
}


/**
 *
 */
int proxenet_ruby_destroy_plugin(plugin_t* plugin)
{
        plugin->state = INACTIVE;
        plugin->pre_function = NULL;
        plugin->post_function = NULL;

	return 0;
}


/**
 *
 */
int proxenet_ruby_destroy_vm(interpreter_t* interpreter)
{
        // TODO: find how to free ruby vm structs

        interpreter->ready = false;
        interpreter->vm = NULL;

	return 0;
}


/**
 *
 */
int proxenet_ruby_initialize_function(plugin_t* plugin, req_t type)
{

	/* checks */
	if (!plugin->name) {
		xlog(LOG_ERROR, "%s\n", "null plugin name");
		return -1;
	}

	if (proxenet_ruby_load_file(plugin) < 0) {
		xlog(LOG_ERROR, "Failed to load %s\n", plugin->filename);
		return -1;
	}

	/* get function ID */
	switch(type) {
		case REQUEST:
			if (plugin->pre_function) {
				xlog(LOG_WARNING, "Pre-hook function already defined for '%s'\n", plugin->name);
				return 0;

			}

			plugin->pre_function  = (void*) rb_intern(CFG_REQUEST_PLUGIN_FUNCTION);
			if (plugin->pre_function) {
#ifdef DEBUG
				xlog(LOG_DEBUG, "Loaded %s:%s\n", plugin->filename, CFG_REQUEST_PLUGIN_FUNCTION);
#endif
				return 0;
			}
			break;

		case RESPONSE:
			if (plugin->post_function) {
				xlog(LOG_WARNING, "Post-hook function already defined for '%s'\n", plugin->name);
				return 0;
			}

			plugin->post_function = (void*) rb_intern(CFG_RESPONSE_PLUGIN_FUNCTION);
			if (plugin->post_function) {
#ifdef DEBUG
				xlog(LOG_DEBUG, "Loaded %s:%s\n", plugin->filename, CFG_RESPONSE_PLUGIN_FUNCTION);
#endif
				return 0;
			}
			break;

		default:
			xlog(LOG_CRITICAL, "%s\n", "Should never be here, autokill !");
			abort();
			break;
	}

	xlog(LOG_ERROR, "%s\n", "Failed to find function");

	return -1;
}


/**
 * this function will safely be executed (gvl acquired)
 * this should not create a blocking/bottleneck situation since the access to rb_mKernel is
 * mutex-protected by proxenet before
 *
 *
 */
static void* _safe_call_func(void* arg)
{
        struct proxenet_ruby_args* args = (struct proxenet_ruby_args*) arg;
        VALUE rRet = rb_funcall2(args->rVM, args->rFunc, 3, args->rArgs);
        return (void*)rRet;
}


/**
 *
 */
static char* proxenet_ruby_execute_function(interpreter_t* interpreter, ID rFunc, request_t* request)
{
	char *buf, *data;
	int buflen, i;
	VALUE rRet;
	char *uri;

        struct proxenet_ruby_args args;

	uri = request->uri;
	if (!uri)
		return NULL;

	/* build args */
	args.rVM = (VALUE)interpreter->vm;

        args.rFunc = rFunc;

	args.rArgs[0] = INT2NUM(request->id);
	args.rArgs[1] = rb_str_new(request->data, request->size);
	args.rArgs[2] = rb_str_new2(uri);

        for(i=0; i<3; i++) {
                rb_gc_register_address(&args.rArgs[i]);
	}

	/* safe function call */
        rRet = (VALUE)_safe_call_func(&args);
	if (!rRet) {
		xlog(LOG_ERROR, "%s\n", "[ruby] funcall2() failed");
		data = NULL;
		goto call_end;
	}

	rb_check_type(rRet, T_STRING);

        for(i=0; i<3; i++) {
                rb_gc_unregister_address(&args.rArgs[i]);
	}


	/* copy result to exploitable buffer */
	buf = RSTRING_PTR(rRet);
	buflen = RSTRING_LEN(rRet);

	data = proxenet_xmalloc(buflen + 1);
	data = memcpy(data, buf, buflen);

	request->data = data;
	request->size = buflen;

call_end:

	return data;
}


/**
 *
 */
static void proxenet_ruby_lock_vm(interpreter_t *interpreter)
{
	pthread_mutex_lock(&interpreter->mutex);
}


/**
 *
 */
static void proxenet_ruby_unlock_vm(interpreter_t *interpreter)
{
	pthread_mutex_unlock(&interpreter->mutex);
}


/**
 *
 */
char* proxenet_ruby_plugin(plugin_t* plugin, request_t* request)
{
	char* buf = NULL;
	interpreter_t *interpreter = plugin->interpreter;
	ID rFunc;

	if (request->type == REQUEST)
		rFunc = (ID) plugin->pre_function;
	else
		rFunc = (ID) plugin->post_function;

        proxenet_ruby_lock_vm(interpreter);
	buf = proxenet_ruby_execute_function(interpreter, rFunc, request);
	proxenet_ruby_unlock_vm(interpreter);
	return buf;
}

#endif /* _RUBY_PLUGIN */

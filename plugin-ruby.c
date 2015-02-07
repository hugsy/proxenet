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

#include "core.h"
#include "plugin.h"
#include "plugin-ruby.h"
#include "utils.h"
#include "main.h"


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

	filename = plugin->filename;

        PROXENET_ABSOLUTE_PLUGIN_PATH(filename, pathname);

#if _RUBY_MINOR_ == 9
	rb_protect(proxenet_ruby_require_cb, (VALUE) pathname, &res);
#elif _RUBY_MINOR_ == 8
	rb_protect(proxenet_ruby_require_cb, (VALUE) pathname, &res);
#else
	abort();
#endif
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
	xlog(LOG_DEBUG, "%s\n", "Initializing Ruby VM");
#endif

	/* init vm */
	ruby_init();

#if (_RUBY_MAJOR_ == 2 || (_RUBY_MAJOR_ == 1 && _RUBY_MINOR_ == 9))
#ifdef DEBUG
	xlog(LOG_DEBUG, "%s\n", "Using Ruby 1.9 C API");
#endif
	interpreter->vm = (void*) rb_cObject;

#elif (_RUBY_MAJOR_ == 1 && _RUBY_MINOR_ == 8)
#ifdef DEBUG
	xlog(LOG_DEBUG, "%s\n", "Using Ruby 1.8 C API");
#endif
	interpreter->vm = (void*) ruby_top_self;

#else
        xlog(LOG_CRITICAL, "%s\n", "Unsupported Ruby version");
	abort();
#endif

	ruby_init_loadpath();

	interpreter->ready = true;

	return 0;
}


/**
 *
 */
int proxenet_ruby_destroy_vm(plugin_t* plugin)
{
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
 *
 */
static char* proxenet_ruby_execute_function(interpreter_t* interpreter, ID rFunc, request_t* request)
{
	char *buf, *data;
	int buflen, i;
	VALUE rArgs[3], rRet, rVM;
	char *uri;

	uri = get_request_full_uri(request);
	if (!uri)
		return NULL;

	/* build args */
	rVM = (VALUE)interpreter->vm;

	rArgs[0] = INT2FIX(request->id);
	rArgs[1] = rb_str_new(request->data, request->size);
	rArgs[2] = rb_str_new2(uri);

        for(i=0; i<3; i++) {
                rb_gc_register_address(&rArgs[i]);
	}

	/* function call */
	rRet = rb_funcall2(rVM, rFunc, 3, rArgs);
	if (!rRet) {
		xlog(LOG_ERROR, "%s\n", "Function call failed");
		data = NULL;
		goto call_end;
	}

	rb_check_type(rRet, T_STRING);

        for(i=0; i<3; i++) {
                rb_gc_unregister_address(&rArgs[i]);
	}

	/* copy result to exploitable buffer */
	buf = RSTRING_PTR(rRet);
	buflen = RSTRING_LEN(rRet);

	data = proxenet_xmalloc(buflen + 1);
	data = memcpy(data, buf, buflen);

	request->data = data;
	request->size = buflen;

call_end:
	proxenet_xfree(uri);
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

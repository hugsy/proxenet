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

#include "plugin.h"
#include "plugin-ruby.h"
#include "utils.h"
#include "main.h"



/**
 *
 */
int proxenet_ruby_load_file(plugin_t* plugin)
{
	char* filename;
	char* pathname;
	size_t pathname_len;
	int res = 0;
	
	filename = plugin->filename;
	
	/* load script */
	pathname_len = strlen(cfg->plugins_path) + 1 + strlen(filename) + 1;
	pathname = alloca(pathname_len + 1);
	proxenet_xzero(pathname, pathname_len + 1);
	snprintf(pathname, pathname_len, "%s/%s", cfg->plugins_path, filename);

#ifdef _RUBY_VERSION_1_9
	rb_protect(RUBY_METHOD_FUNC(rb_require), (VALUE) pathname, &res);
#else
	rb_protect(rb_require, (VALUE) pathname, &res);
#endif
	if (res != 0) {
		xlog(LOG_ERROR, "[Ruby] Error %d when load file '%s'\n", res, pathname);
		return -1;
	}

	xlog(LOG_DEBUG, "%s\n", pathname);

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
	
#ifdef _RUBY_VERSION_1_9
#ifdef DEBUG
	xlog(LOG_DEBUG, "%s\n", "Using Ruby 1.9 C API");
#endif
	/* luke : not great, but temporary until I get rb_vm_top_self working */
	interpreter->vm = (void*) rb_cObject;
#else
	
#ifdef DEBUG
	xlog(LOG_DEBUG, "%s\n", "Using Ruby 1.8 C API");
#endif
	interpreter->vm = (void*) ruby_top_self;
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
int proxenet_ruby_initialize_function(plugin_t* plugin, int type)
{
	
	/* checks */
	if (!plugin->name) {
		xlog(LOG_ERROR, "%s\n", "null plugin name");
		return -1;
	}

	if (plugin->pre_function && type == REQUEST) {
		xlog(LOG_WARNING, "Pre-hook function already defined for '%s'\n", plugin->name);
		return 0;
	}

	if (plugin->post_function && type == RESPONSE) {
		xlog(LOG_WARNING, "Post-hook function already defined for '%s'\n", plugin->name);
		return 0;
	}
	
	if (proxenet_ruby_load_file(plugin) < 0) {
		xlog(LOG_ERROR, "Failed to load %s\n", plugin->filename);
		return -1;
	}

	/* get function ID */
	if (type == REQUEST) {
		plugin->pre_function  = (void*) rb_intern(CFG_REQUEST_PLUGIN_FUNCTION);
		if (plugin->pre_function) {
#ifdef DEBUG
			xlog(LOG_DEBUG, "Loaded %s:%s\n", plugin->filename, CFG_REQUEST_PLUGIN_FUNCTION);
#endif
			return 0;
		}
		
	} else {
		plugin->post_function = (void*) rb_intern(CFG_RESPONSE_PLUGIN_FUNCTION);
		if (plugin->post_function) {
#ifdef DEBUG
			xlog(LOG_DEBUG, "Loaded %s:%s\n", plugin->filename, CFG_RESPONSE_PLUGIN_FUNCTION);
#endif			
			return 0;
		}
	}

	xlog(LOG_ERROR, "%s\n", "Failed to find function");
	return -1;
}


/**
 *
 */
char* proxenet_ruby_execute_function(interpreter_t* interpreter, ID rFunc, long rid, char* request_str)
{
	char *buf, *data;
	int buflen;
	VALUE rArgs[2], rRet, rVM;
	
	/* build args */
	rVM = (VALUE)interpreter->vm;

	rArgs[0] = INT2FIX(rid);
	rArgs[1] = rb_str_new2(request_str);
	
	/* function call */
	rRet = rb_funcall2(rVM, rFunc, 2, rArgs); 
	if (!rRet) {
		xlog(LOG_ERROR, "%s\n", "Function call failed");
		return NULL;
	}
	
	/* todo catch TypeError except */
	/* rb_ary_push(rArgs, rRet); */
	/* rb_ary_push(rArgs, T_STRING); */
	/* rb_protect(rb_check_type, rArgs, &err); */

	/* copy result to exploitable buffer */
	buf = RSTRING_PTR(rRet);
	buflen = RSTRING_LEN(rRet);
	
	data = proxenet_xmalloc(buflen + 1);
	memcpy(data, buf, buflen);
	
	return data;
}


/**
 *
 */
void proxenet_ruby_lock_vm(interpreter_t *interpreter)
{
	pthread_mutex_lock(&interpreter->mutex);
}


/**
 *
 */
void proxenet_ruby_unlock_vm(interpreter_t *interpreter)
{
	pthread_mutex_unlock(&interpreter->mutex);
}


/**
 * 
 */
char* proxenet_ruby_plugin(plugin_t* plugin, long rid, char* request, int type)
{
	char* buf = NULL;
	interpreter_t *interpreter = plugin->interpreter;
	ID rFunc;
	
	if (type == REQUEST)
		rFunc = (ID) plugin->pre_function;
	else 
		rFunc = (ID) plugin->post_function;

	proxenet_ruby_lock_vm(interpreter);
		buf = proxenet_ruby_execute_function(interpreter, rFunc, rid, request);
	proxenet_ruby_unlock_vm(interpreter);
	
	return buf;
}

#endif /* _RUBY_PLUGIN */

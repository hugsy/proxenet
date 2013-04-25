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
int proxenet_ruby_initialize_vm(plugin_t* plugin)
{
	interpreter_t *interpreter;

	interpreter = plugin->interpreter;

	/* checks */
	if (interpreter->ready)
		return 0;
	
	/* init vm */
	ruby_init();
		
	interpreter->vm = rb_vm_top_self();
	if (!interpreter->vm) 
		return -1;

	interpreter->is_ready = TRUE;
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
	char* pathname;
	size_t pathname_len;
	
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
	
	/* load script */
	pathname_len = strlen(cfg->plugins_path) + 1 + strlen(plugin->name);
	pathname = alloca(pathname_len + 1);
	xzero(pathname, pathname_len + 1);
	snprintf(pathname, pathname_len, "%s/%s", cfg->plugins_path, plugin->name);

	if (rb_load_file(pathname) == QFalse) {
		return -1;
	}
	
	/* get function pointers */
	if (type==REQUEST) {
		plugin->pre_function  = (void*) rb_intern(CFG_REQUEST_PLUGIN_FUNCTION);
		if (plugin->pre_function) 
			return 0;
	} else {
		plugin->post_function = (void*) rb_intern(CFG_RESPONSE_PLUGIN_FUNCTION);
		if (plugin->post_function) 
			return 0;
	}

	xlog(LOG_ERROR, "%s\n", "Failed to find function");
	return -1;
}


/**
 *
 */
char* proxenet_ruby_execute_function(plugin_t* plugin, long rid, char* request_str, int type)
{
	char *buf;
	int buflen;
	interpreter_t *interpreter;
	ID rFunc;
	VALUE rArgs[2];
	VALUE rReqStr;
	VALUE rRet;

	interpreter = plugin->interpreter;
	
	/* build args */
	rReqId  = INT2FIX(rid);
	rReqStr = rb_str_new2(request_str);

	rArgs[0] = rReqId;
	rArgs[1] = rReqStr;

	/* function call */
	if (type == REQUEST)
		rFunc = (ID) plugin->pre_function;
	else 
		rFunc = (ID) plugin->post_function;

	
	rRet = rb_funcall(interpreter->vm, rFunc, 2, args);
	
	if (!rRet) {
		xlog(LOG_ERROR, "%s\n", "Function call failed");
		return NULL;
	}
	if (!Check_Type(rRet, T_STRING)) {
		xlog(LOG_ERROR, "%s\n", "Invalid return type");
		return NULL;
	}

	/* copy result to exploitable buffer */
	buf = RSTRING_PTR(rRet);
	buflen = RSTRING_LEN(rRet);
	
	data = xmalloc(buflen + 1);
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
	
	proxenet_ruby_lock_vm(interpreter);
	buf = proxenet_ruby_execute_function(plugin, rid, request, type);
	proxenet_ruby_unlock_vm(interpreter);
	
	return buf;
}

#endif /* _RUBY_PLUGIN */

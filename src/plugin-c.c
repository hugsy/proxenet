#include <config.h>

#ifdef _C_PLUGIN

/*******************************************************************************
 *
 * C plugin 
 *
 */

#include <dlfcn.h>
#include <string.h>
#include <alloca.h>

#include "plugin.h"
#include "utils.h"
#include "main.h"


/**
 *
 */
int proxenet_c_initialize_vm(plugin_t* plugin) 
{
	size_t pathlen;
	char *pathname;
	void *interpreter;
	
	pathlen = strlen(cfg->plugins_path) + 1 + strlen(plugin->filename) + 1;
	pathname = alloca(pathlen + 1);
	proxenet_xzero(pathname, pathlen+1);
	
	if (snprintf(pathname, pathlen, "%s/%s", cfg->plugins_path, plugin->filename) < 0) {
		xlog(LOG_CRITICAL,"%s\n", "Failed to get path");
		return -1;
	}


	interpreter = dlopen(pathname, RTLD_NOW);
	if (!interpreter) {
		xlog(LOG_CRITICAL,"[C] %s\n", dlerror());
		return -1;
	}

	plugin->interpreter = interpreter;

	return 0;
}


/**
 *
 */
int proxenet_c_destroy_vm(plugin_t* plugin)
{
	void *interpreter;
	interpreter = (void *) plugin->interpreter;

	if (interpreter) {
		plugin->pre_function  = NULL;
		plugin->post_function = NULL;		
		
		if (dlclose(interpreter) < 0) {
			xlog(LOG_ERROR, "[C] Failed to close interpreter for %s\n", plugin->name); 
			return -1;
		}
		
		interpreter = NULL;
		return 0;
		
	} else {
		xlog(LOG_ERROR, "%s\n", "[C] Cannot destroy uninitialized interpreter");
		return -1;
	}
}


/**
 *
 */
int proxenet_c_initialize_function(plugin_t* plugin, int type)
{
	void *interpreter;

	/* if already initialized, return ok */
	if (plugin->pre_function && type==REQUEST)
		return 0;
	
	if (plugin->post_function && type==RESPONSE)
		return 0;
	
	interpreter = (void *) plugin->interpreter;

	if (type == REQUEST) {
		plugin->pre_function = dlsym(interpreter, CFG_REQUEST_PLUGIN_FUNCTION);
		if (plugin->pre_function) {
#ifdef DEBUG
			xlog(LOG_DEBUG, "[C] Pre func is at %p\n", plugin->pre_function);
#endif			
			return 0;
		}
		
	} else {
		plugin->post_function = dlsym(interpreter, CFG_RESPONSE_PLUGIN_FUNCTION);
		if (plugin->post_function) {
#ifdef DEBUG
			xlog(LOG_DEBUG, "[C] Post func is at %p\n", plugin->post_function);
#endif
			return 0;
		}
		
	}
	
	xlog(LOG_ERROR, "[C] dlsym failed: %s\n", dlerror());
	return -1;
}


/**
 *
 */
char* proxenet_c_plugin(plugin_t *plugin, unsigned long request_id, char *request, size_t* request_size, int type)
{
	char* (*plugin_function)(unsigned long, char*);
	char *bufres;

	bufres = NULL;
	
	if (type == REQUEST)
		plugin_function = plugin->pre_function;
	else
		plugin_function = plugin->post_function;

	bufres = (*plugin_function)(request_id, request);
	if(!bufres)
		return request;

	*request_size = strlen(bufres);
	
	return bufres;
}

#endif

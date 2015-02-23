#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _C_PLUGIN

/*******************************************************************************
 *
 * C plugin
 *
 */

#include <dlfcn.h>
#include <string.h>
#include <alloca.h>

#include "core.h"
#include "utils.h"
#include "main.h"
#include "plugin.h"


/**
 *
 */
int proxenet_c_initialize_vm(plugin_t* plugin)
{
	void *interpreter;

	interpreter = dlopen(plugin->fullpath, RTLD_NOW);
	if (!interpreter) {
		xlog(LOG_ERROR, "Failed to dlopen('%s'): %s\n", plugin->fullpath, dlerror());
		return -1;
	}

	plugin->interpreter = interpreter;
        plugin->interpreter->ready = true;

	return 0;
}


/**
 *
 */
int proxenet_c_destroy_vm(plugin_t* plugin)
{
	void *interpreter;

        if (!plugin->interpreter->ready){
                xlog(LOG_ERROR, "%s\n", "Cannot destroy un-init dl link");
                return -1;
        }

	interpreter = (void *) plugin->interpreter;

        plugin->pre_function  = NULL;
        plugin->post_function = NULL;

        if (dlclose(interpreter) < 0) {
                xlog(LOG_ERROR, "Failed to dlclose() for '%s': %s\n", plugin->name, dlerror());
                return -1;
        }

        plugin->interpreter->ready = false;
        plugin->interpreter = NULL;
        return 0;
}


/**
 *
 */
int proxenet_c_initialize_function(plugin_t* plugin, req_t type)
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
char* proxenet_c_plugin(plugin_t *plugin, request_t *request)
{
	char* (*plugin_function)(unsigned long, char*, char*);
	char *bufres, *uri;

	bufres = uri = NULL;

        uri = get_request_full_uri(request);
	if (!uri)
		return NULL;

	if (request->type == REQUEST)
		plugin_function = plugin->pre_function;
	else
		plugin_function = plugin->post_function;

	bufres = (*plugin_function)(request->id, request->data, uri);
	if(!bufres){
                request->size = -1;
                goto end_exec_c_plugin;
        }

	request->size = strlen(bufres);

end_exec_c_plugin:
        proxenet_xfree(uri);
	return bufres;
}

#endif

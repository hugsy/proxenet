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

#ifdef __LINUX__
#include <alloca.h>
#endif

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

	plugin->interpreter->vm = interpreter;
        plugin->interpreter->ready = true;

	return 0;
}


/**
 *
 */
int proxenet_c_destroy_plugin(plugin_t* plugin)
{
        plugin->state = INACTIVE;
        plugin->pre_function  = NULL;
        plugin->post_function = NULL;

        if (dlclose((void*)plugin->interpreter->vm) < 0) {
                xlog(LOG_ERROR, "Failed to dlclose() for '%s': %s\n", plugin->name, dlerror());
                return -1;
        }

        return 0;
}


/**
 *
 */
int proxenet_c_destroy_vm(interpreter_t* interpreter)
{
        interpreter->ready = false;
        interpreter = NULL;
        return 0;
}


/**
 *
 */
int proxenet_c_initialize_function(plugin_t* plugin, req_t type)
{
	void *interpreter;

        if (plugin->interpreter==NULL || plugin->interpreter->ready==false){
                xlog(LOG_ERROR, "%s\n", "[c] not ready (dlopen() failed?)");
                return -1;
        }

        interpreter = (void *) plugin->interpreter->vm;

	/* if already initialized, return ok */
	if (plugin->pre_function && type==REQUEST)
		return 0;

	if (plugin->post_function && type==RESPONSE)
		return 0;


	if (type == REQUEST) {
		plugin->pre_function = dlsym(interpreter, CFG_REQUEST_PLUGIN_FUNCTION);
		if (plugin->pre_function) {
#ifdef DEBUG
			xlog(LOG_DEBUG, "[C] '%s' request_hook function is at %p\n",
                             plugin->name,
                             plugin->pre_function);
#endif
			return 0;
		}

	} else {
		plugin->post_function = dlsym(interpreter, CFG_RESPONSE_PLUGIN_FUNCTION);
		if (plugin->post_function) {
#ifdef DEBUG
			xlog(LOG_DEBUG, "[C] '%s' response_hook function is at %p\n",
                             plugin->name,
                             plugin->post_function);
#endif
			return 0;
		}

	}

        xlog(LOG_ERROR, "[C] dlsym(%s) failed for '%s': %s\n",
             (type==REQUEST)?"REQUEST":"RESPONSE",
             plugin->name,
             dlerror());

	return -1;
}


/**
 * Execute a proxenet plugin written in C.
 *
 * @note Because there is no other consistent way in C of keeping track of the
 * right size of the request (strlen() will break at the first NULL byte), the
 * signature of functions in a C plugin must include a pointer to the size which
 * **must** be changed by the called plugin.
 * Therefore the definition is
 * char* proxenet_request_hook(unsigned int rid, char* buf, char* uri, size_t* buflen);
 *
 * See examples/ for examples.
 */
char* proxenet_c_plugin(plugin_t *plugin, request_t *request)
{
	char* (*plugin_function)(unsigned long, char*, char*, size_t*);
	char *bufres, *uri;
        size_t buflen;

	bufres = uri = NULL;

        uri = request->http_infos.uri;
	if (!uri)
		return NULL;

	if (request->type == REQUEST)
		plugin_function = plugin->pre_function;
	else
		plugin_function = plugin->post_function;

        buflen = request->size;
	bufres = (*plugin_function)(request->id, request->data, uri, &buflen);
	if(!bufres){
                request->size = -1;
                goto end_exec_c_plugin;
        }

        request->data = proxenet_xstrdup(bufres, buflen);
        request->size = buflen;

end_exec_c_plugin:
	return request->data;
}

#endif

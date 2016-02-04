#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _TCL_PLUGIN

/*******************************************************************************
 *
 * Tcl plugin
 *
 */

#include <tcl.h>
#include <string.h>
#include <errno.h>

#include "core.h"
#include "utils.h"
#include "main.h"
#include "plugin.h"

#define xlog_tcl(t, ...) xlog(t, "["_TCL_VERSION_"] " __VA_ARGS__)


/**
 *
 */
int proxenet_tcl_load_file(plugin_t* plugin)
{
	char* pathname;
	Tcl_Interp* tcl_interpreter;

        if(plugin->state != INACTIVE){
#ifdef DEBUG
                if(cfg->verbose > 2)
                        xlog_tcl(LOG_DEBUG, "Plugin '%s' is already loaded. Skipping...\n", plugin->name);
#endif
                return 0;
        }

        pathname = plugin->fullpath;
	tcl_interpreter = (Tcl_Interp*) plugin->interpreter->vm;

        if (Tcl_EvalFile (tcl_interpreter, pathname) != TCL_OK){
		xlog_tcl(LOG_ERROR, "Failed to load '%s'\n", pathname);
		return -1;
	}

        plugin->interpreter->vm = tcl_interpreter;
	plugin->interpreter->ready = true;


        tcl_cmds_ptr = Tcl_NewListObj (0, NULL);
        Tcl_IncrRefCount(tcl_cmds_ptr);
        Tcl_ListObjAppendElement(tcl_interpreter, tcl_cmds_ptr, Tcl_NewStringObj(CFG_ONLEAVE_PLUGIN_FUNCTION, -1));

        if (Tcl_EvalObjEx(tcl_interpreter, tcl_cmds_ptr, TCL_EVAL_DIRECT) != TCL_OK) {
                xlog_tcl(LOG_WARNING, "%s() failed to execute properly\n", CFG_ONLOAD_PLUGIN_FUNCTION);
        }

        Tcl_DecrRefCount(tcl_cmds_ptr);

	return 0;
}


/**
 *
 */
int proxenet_tcl_initialize_vm(plugin_t* plugin)
{
	interpreter_t* interpreter;
	Tcl_Interp* tcl_interpreter;
        Tcl_Obj* tcl_cmds_ptr;

	interpreter = plugin->interpreter;

	if (interpreter->ready)
		return 0;

	tcl_interpreter = Tcl_CreateInterp();
        if (!tcl_interpreter)
                return -1;

        Tcl_Init(tcl_interpreter);

	return 0;
}


/**
 *
 */
int proxenet_tcl_destroy_plugin(plugin_t* plugin)
{
        Tcl_Interp* tcl_interpreter;
        Tcl_Obj* tcl_cmds_ptr;

	tcl_interpreter = (Tcl_Interp*)plugin->interpreter->vm;

        proxenet_plugin_set_state(plugin, INACTIVE);

        tcl_cmds_ptr = Tcl_NewListObj (0, NULL);
        Tcl_IncrRefCount(tcl_cmds_ptr);
        Tcl_ListObjAppendElement(tcl_interpreter, tcl_cmds_ptr, Tcl_NewStringObj(CFG_ONLEAVE_PLUGIN_FUNCTION, -1));

        if (Tcl_EvalObjEx(tcl_interpreter, tcl_cmds_ptr, TCL_EVAL_DIRECT) != TCL_OK) {
                xlog_tcl(LOG_WARNING, "%s() failed to execute properly\n", CFG_ONLEAVE_PLUGIN_FUNCTION);
        }

        Tcl_DecrRefCount(tcl_cmds_ptr);

        plugin->pre_function = NULL;
        plugin->post_function = NULL;
        return 0;
}


/**
 *
 */
int proxenet_tcl_destroy_vm(interpreter_t* interpreter)
{
        Tcl_Interp* tcl_interpreter;

	tcl_interpreter = (Tcl_Interp*)interpreter->vm;

        Tcl_DeleteInterp(tcl_interpreter);
        if(Tcl_InterpDeleted(tcl_interpreter)){
                xlog_tcl(LOG_CRITICAL, "An error occured when deleting TCL interpreter: %s", strerror(errno));
                return -1;
        }

	interpreter->ready = false;
        interpreter->vm = NULL;

	return 0;
}


/**
 *
 */
static char* proxenet_tcl_execute_function(interpreter_t* interpreter, request_t *request)
{
	char *buf, *uri;
        Tcl_Interp* tcl_interpreter;
        Tcl_Obj* tcl_cmds_ptr;
	size_t len;
        int i;

	uri = request->http_infos.uri;
	if (!uri)
		return NULL;

	tcl_interpreter = (Tcl_Interp*) interpreter->vm;

        /* create the list of commands to be executed by TCL interpreter */
        tcl_cmds_ptr = Tcl_NewListObj (0, NULL);
        Tcl_IncrRefCount(tcl_cmds_ptr);
        if (request->type == REQUEST)
                Tcl_ListObjAppendElement(tcl_interpreter, tcl_cmds_ptr, Tcl_NewStringObj(CFG_REQUEST_PLUGIN_FUNCTION, -1));
        else
                Tcl_ListObjAppendElement(tcl_interpreter, tcl_cmds_ptr, Tcl_NewStringObj(CFG_RESPONSE_PLUGIN_FUNCTION, -1));

        /* pushing arguments */
        Tcl_ListObjAppendElement(tcl_interpreter, tcl_cmds_ptr, Tcl_NewIntObj(request->id));
        Tcl_ListObjAppendElement(tcl_interpreter, tcl_cmds_ptr, Tcl_NewStringObj(request->data, request->size));
        Tcl_ListObjAppendElement(tcl_interpreter, tcl_cmds_ptr, Tcl_NewStringObj(uri, -1));


        /* execute the commands */
	if (Tcl_EvalObjEx(tcl_interpreter, tcl_cmds_ptr, TCL_EVAL_DIRECT) != TCL_OK) {
                return NULL;
        }

        /* get the result */
        Tcl_DecrRefCount(tcl_cmds_ptr);
        buf = Tcl_GetStringFromObj( Tcl_GetObjResult(tcl_interpreter), &i);
        if (!buf || i<=0)
                return NULL;

        len = (size_t)i;
	buf = proxenet_xstrdup(buf, len);
	if (!buf)
		return NULL;

	request->size = len;
	return buf;
}

/**
 *
 */
static void proxenet_tcl_lock_vm(interpreter_t *interpreter)
{
	pthread_mutex_lock(&interpreter->mutex);
}


/**
 *
 */
static void proxenet_tcl_unlock_vm(interpreter_t *interpreter)
{
	pthread_mutex_unlock(&interpreter->mutex);
}

/**
 *
 */
char* proxenet_tcl_plugin(plugin_t* plugin, request_t *request)
{
	char* buf = NULL;
	interpreter_t *interpreter = plugin->interpreter;

	proxenet_tcl_lock_vm(interpreter);
	buf = proxenet_tcl_execute_function(interpreter, request);
	proxenet_tcl_unlock_vm(interpreter);

	return buf;
}

#endif /* _TCL_PLUGIN */

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

#include <alloca.h>
#include <string.h>
#include <errno.h>

#include "core.h"
#include "utils.h"
#include "main.h"
#include "plugin.h"


/**
 *
 */
int proxenet_tcl_load_file(plugin_t* plugin)
{
	char* filename;
	char* pathname;

	Tcl_Interp* tcl_interpreter;

	filename = plugin->filename;
	tcl_interpreter = (Tcl_Interp*) plugin->interpreter->vm;

	PROXENET_ABSOLUTE_PLUGIN_PATH(filename, pathname);

        if (Tcl_EvalFile (tcl_interpreter, pathname) != TCL_OK){
		xlog(LOG_ERROR, "Failed to load '%s'\n", pathname);
		return -1;
	}

	return 0;
}


/**
 *
 */
int proxenet_tcl_initialize_vm(plugin_t* plugin)
{
	interpreter_t* interpreter;
	Tcl_Interp* tcl_interpreter;

	interpreter = plugin->interpreter;

	if (interpreter->ready)
		return 0;

	tcl_interpreter = Tcl_CreateInterp();
        if (!tcl_interpreter)
                return -1;

        Tcl_Init(tcl_interpreter);

	plugin->interpreter->vm = tcl_interpreter;
	plugin->interpreter->ready = true;

	return 0;
}


/**
 *
 */
int proxenet_tcl_destroy_vm(plugin_t* plugin)
{
	interpreter_t* interpreter;
        Tcl_Interp* tcl_interpreter;

        /* cannot destroy interpreter while active */
	if(count_plugins_by_type(_TCL_) > 0)
		return -1;

	interpreter = plugin->interpreter;
	tcl_interpreter = (Tcl_Interp*)interpreter->vm;

        Tcl_DeleteInterp(tcl_interpreter);
        if(Tcl_InterpDeleted(tcl_interpreter)){
                xlog(LOG_CRITICAL, "An error occured when deleting TCL interpreter: %s", strerror(errno));
                return -1;
        }
	interpreter->ready = false;

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

	uri = get_request_full_uri(request);
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

	proxenet_xfree(uri);

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

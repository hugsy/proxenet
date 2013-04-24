#ifdef _PYTHON_PLUGIN

/*******************************************************************************
 *
 * Python plugin implementation
 *
 * tested on CPython2.6, CPython2.7
 *
 */

#include <Python.h>

#include <string.h>
#include <alloca.h>
#include <pthread.h>

#include "main.h"
#include "plugin.h"
#include "utils.h"
#include "plugin-python.h"


/**
 *
 */
void proxenet_python_append_path(interpreter)
{
	PyObject *pPath, *pAddPath;

	pPath = PySys_GetObject("path");
	pAddPath = PyString_FromString(cfg->plugins_path);
	
	if (pAddPath) {
		PyList_Insert(pPath, 0, pAddPath);
		Py_DECREF(pAddPath);
	}

}


/**
 *
 */
void proxenet_python_initialize_vm(plugin_t* plugin) 
{
	interpreter_t *interpreter = plugin->interpreter;
	
	/* is vm initialized ? */
	if (interpreter->ready)
		return;
	
#ifdef DEBUG
	xlog(LOG_DEBUG, "%s\n", "Initializing Python VM");
#endif
		
	Py_Initialize();
	if (Py_IsInitialized() == FALSE) {
		xlog(LOG_CRITICAL, "%s\n", "Failed to initialize Python engine");
		plugin->interpreter->vm = NULL;
		return;
	}

	proxenet_python_append_path(interpreter);
	
	interpreter->ready = TRUE;
}


/**
 *
 */
void proxenet_python_destroy_vm(plugin_t* plugin)
{
	if (! Py_IsInitialized()) {
		xlog(LOG_CRITICAL, "%s\n", "Python VM should not be uninitialized here");
		abort();
	}
	
	if(count_plugins_by_type(PYTHON) == 0) {
		Py_Finalize();
	}

	plugin->interpreter->ready = FALSE;
	return;
}



/**
 *
 */
int proxenet_python_initialize_function(plugin_t* plugin, char type) 
{
	char*	module_name;
	size_t 	module_name_len;
	PyObject *pModStr, *pMod, *pFunc;
	const char* function_name;
	boolean is_request = (type==REQUEST) ? TRUE : FALSE;
	
	/* checks  */
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

	function_name = is_request ? CFG_REQUEST_PLUGIN_FUNCTION : CFG_RESPONSE_PLUGIN_FUNCTION;
	
	/* import module */
	module_name_len = strlen(plugin->name) + 2; 
	module_name = alloca(module_name_len);
	xzero(module_name, module_name_len);
	snprintf(module_name, module_name_len, "%d%s", plugin->id, plugin->name);
	
	pModStr = PyString_FromString(module_name);
	if (!pModStr) {
		PyErr_Print();
		return -1;
	}

#ifdef DEBUG
	xlog(LOG_DEBUG, "Importing '%s'\n", module_name);
#endif
	
	pMod = PyImport_Import(pModStr);
	if(!pMod) {
		PyErr_Print(); 
		xlog(LOG_WARNING, "Is '%s' in PYTHONPATH ?\n", cfg->plugins_path);
		Py_DECREF(pModStr);
		return -1;
	}

	Py_DECREF(pModStr);

#ifdef DEBUG
	xlog(LOG_DEBUG, "Fetching '%s:%s'\n", module_name, function_name);
#endif	

	/* find reference to function in module */
	pFunc = PyObject_GetAttrString(pMod, function_name);
	if (!pFunc) {
		PyErr_Print(); 
		return -1;
	}

	if (!PyCallable_Check(pFunc)) {
		xlog(LOG_ERROR, "Object in %s is not callable\n", module_name);
		return -1;
	}

	if (is_request)
		plugin->pre_function = pFunc;
	else 
		plugin->post_function = pFunc;

	return 0;
}


/**
 *
 */
char* proxenet_python_execute_function(PyObject* pFuncRef, long rid, char* request_str)
{
	PyObject *pArgs, *pResult;
	char *buffer, *result;
	int ret;
	Py_ssize_t len;

	result = buffer = NULL;
	len = -1;
	
	pArgs = Py_BuildValue("is", rid, request_str);
	if (!pArgs) {
		xlog(LOG_ERROR, "%s\n", "Failed to build args");
		PyErr_Print();
		return result;
	}

	pResult = PyObject_CallObject(pFuncRef, pArgs);
	if (!pResult) {
		xlog(LOG_ERROR, "%s\n", "Failed during func call");
		PyErr_Print();
		return result;
	}
	
	/* if (PyString_Check(pResult)){  // supprime dans python2.7 ?? */
	if (PyBytes_Check(pResult)) {
		ret = PyString_AsStringAndSize(pResult, &buffer, &len);
		if (ret < 0) {
			PyErr_Print();
			
		} else {
			fprintf(stderr, "%s", buffer);

			result = (char*) xmalloc(len+1);
			result = memcpy(result, buffer, len);
		}
		
	} else {
		xlog(LOG_ERROR, "%s\n", "Incorrect return type (not string)");
		result = NULL;
	}
	
	Py_DECREF(pResult);
	
	return result;
}


/**
 *
 */
void proxenet_python_lock_vm(plugin_t *plugin)
{
	pthread_mutex_lock(&plugin->interpreter->mutex);

}


/**
 *
 */
void proxenet_python_unlock_vm(plugin_t *plugin)
{
	pthread_mutex_unlock(&plugin->interpreter->mutex);
}


/**
 * 
 */
char* proxenet_python_plugin(plugin_t* plugin, long rid, char* request, char type)
{	
	char *dst_buf = NULL;
	PyObject *pFunc = NULL;
	boolean is_request = (type==REQUEST) ? TRUE : FALSE;

	
	if (!plugin->interpreter->ready)
		return request;

#ifdef DEBUG
	xlog(LOG_DEBUG, "%s\n", "Trying to lock PyVM");
#endif	

	proxenet_python_lock_vm(plugin);

#ifdef DEBUG
	xlog(LOG_DEBUG, "%s\n", "PyVM locked");
#endif
       
	if (is_request)
		pFunc = (PyObject*) plugin->pre_function;
	else
		pFunc = (PyObject*) plugin->post_function;

	dst_buf = proxenet_python_execute_function(pFunc, rid, request);
	if (!dst_buf) {
		xlog(LOG_ERROR,
		     "[%s] Error while executing plugin on %s\n",
		     plugin->name,
		     is_request ? "Request" : "Reponse");
		
		dst_buf = request;
	}
	
	/* Py_DECREF(pFunc); */

#ifdef DEBUG
	xlog(LOG_DEBUG, "%s\n", "Unlocking PyVM");
#endif	
	proxenet_python_unlock_vm(plugin);
	
	return dst_buf;
}

#endif /* _PYTHON_PLUGIN */

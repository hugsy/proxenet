#ifdef _PYTHON_PLUGIN

/*******************************************************************************
 *
 * Python plugin implementation
 *
 * https://mdavey.wordpress.com/2006/02/09/python-rule-for-py_newinterpreter/
 */

#include <Python.h>

#include <string.h>
#include <alloca.h>

#include "main.h"
#include "plugin.h"
#include "utils.h"
#include "plugin-python.h"


static PyThreadState *proxenet_main_interpreter;

/**
 *
 */
void proxenet_python_initialize_vm(plugin_t* plugin) 
{
	/* PyThreadState* pOldCtx = NULL; */
	
	if(count_initialized_plugins_by_type(PYTHON) == 0){
#ifdef DEBUG
		xlog(LOG_DEBUG, "%s\n", "Initializing Python VM");
#endif
		Py_Initialize();
		if (Py_IsInitialized() == FALSE) {
			xlog(LOG_CRITICAL, "%s\n", "Failed to initialize Python engine");
			plugin->interpreter = NULL;
			return;
		}
		
		proxenet_main_interpreter = PyThreadState_Get();
		
		PyEval_InitThreads();
	}
	
	/* a new interpreter does not have argv (required by many modules), we need to add one */
	wchar_t *PyArgv = "proxenet_python_plugin_engine";
	
	plugin->interpreter = (void*)Py_NewInterpreter();
	if (plugin->interpreter == NULL) {
		xlog(LOG_CRITICAL, "%s\n", "Failed to create new python sub-interpreter");
		abort();
	}
	PySys_SetArgv(1, &PyArgv);

}


/**
 *
 */
void proxenet_python_destroy_vm(plugin_t* plugin)
{
	if (!Py_IsInitialized() || !PyEval_ThreadsInitialized())
		return;
	
	PyThreadState_Swap((PyThreadState *)plugin->interpreter);
	Py_EndInterpreter((PyThreadState *)plugin->interpreter);
	
	PyThreadState_Swap(proxenet_main_interpreter);
	
	plugin->interpreter = NULL;
	
	if(count_plugins_by_type(PYTHON) == 0) {
		Py_Finalize();
	}
	
	return;
}


/**
 *
 */
PyObject* proxenet_python_load_function(plugin_t* plugin, const char* function_name) 
{     
	PyObject *pModStr, *pMod, *pFunc;

#ifdef DEBUG
	xlog(LOG_DEBUG, "Loading %s.%s\n", plugin->name, function_name);
#endif

	/* import module */
	if (!plugin->name) {
		xlog(LOG_ERROR, "%s\n", "null plugin name");
		return NULL;
	}
	
	pModStr = PyString_FromString(plugin->name);
	if (!pModStr) {
		PyErr_Print();
		return NULL;
	}

	pMod 	= PyImport_Import(pModStr);
	Py_DECREF(pModStr);
	if(!pMod) {
		PyErr_Print(); 
		xlog(LOG_INFO, "Is '%s' in PYTHONPATH ?\n", cfg->plugins_path);
		return NULL;
	}

	/* find reference to function in module */
	pFunc = PyObject_GetAttrString(pMod, function_name);
	if (!pFunc) {
		PyErr_Print(); 
		return NULL;
	}

	if (PyCallable_Check(pFunc)) {
		xlog(LOG_ERROR, "Object '%s' in %s is not callable\n", function_name, plugin->name);
		return NULL;
	}

	return pFunc;
}


/**
 *
 */
char* proxenet_python_execute_function(PyObject* pFuncRef, char* http_request)
{
	PyObject *pResult;
	char *result;

	pResult = PyObject_CallFunction(pFuncRef, "s", http_request);
	
	if (PyBytes_Check(pResult)){
		result = strdup(PyString_AsString(pResult));
		
	} else {
		xlog(LOG_ERROR, "%s\n", "Incorrect return type (not string)");
		result = NULL;
	}
	
	Py_DECREF(pResult);
	
	return result;
}


/**
 * loads lib, get func ref, execute func, memcpy result
 */
char* proxenet_python_plugin(plugin_t* plugin, char* request, const char* function_name)
{
#ifdef DEBUG
	xlog(LOG_DEBUG, "Init %s:%s\n", plugin->name, function_name);
#endif
	
	char *dst_buf = NULL;
	PyObject *pFunc = NULL;
	
	if (!plugin->interpreter)
		return request;
	
	PyThreadState_Swap(plugin->interpreter);
	
	pFunc  = proxenet_python_load_function(plugin, function_name);
#ifdef DEBUG
	xlog(LOG_DEBUG, "Func '%s' -> %p\n", function_name, pFunc);
#endif
	if (!pFunc) {
		xlog(LOG_ERROR, "Loading %s:%s failed\n", plugin->name, function_name);
		return request;
	}
	
	dst_buf = proxenet_python_execute_function(pFunc, request);
	if (!dst_buf) {
		xlog(LOG_ERROR,"[%s] Error while executing '%s'\n", plugin->name, dst_buf);
		return request;
	}
	
#ifdef DEBUG
	xlog(LOG_DEBUG, "End %s:%s\n", plugin->name, function_name);
#endif

	Py_DECREF(pFunc);

	PyThreadState_Swap(proxenet_main_interpreter);

	return dst_buf;
}

#endif

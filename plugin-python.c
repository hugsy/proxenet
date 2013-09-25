#ifdef _PYTHON_PLUGIN

/*******************************************************************************
 *
 * Python plugin implementation
 *
 * tested on
 * - CPython2.6
 * - CPython2.7
 * - CPython3.3
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

#if _PYTHON_MAJOR_ == 3
#define PYTHON_CHECK PyBytes_Check
#define PYTHON_FROMSTRING PyUnicode_FromString
#define PYTHON_ASSTRINGANDSIZE PyBytes_AsStringAndSize

# else /* if defined _PYTHON2_ */
#define PYTHON_CHECK PyString_Check
#define PYTHON_ASSTRINGANDSIZE  PyString_AsStringAndSize
/* #define PYTHON_CHECK PyByteArray_Check */
/* #define PYTHON_ASSTRINGANDSIZE PyByteArray_AsString */
#define PYTHON_FROMSTRING  PyString_FromString

#endif

/**
 *
 */
int proxenet_python_append_path(interpreter_t *interpreter)
{
	PyObject *pPath, *pAddPath;
	int retcode = 0;
	
	pPath = PySys_GetObject("path");
	if (!pPath) {
		xlog(LOG_ERROR, "%s\n", "Failed to find `sys.path'");
		return -1;
	}

	if (!PyList_Check(pPath)) {
		return -1;
	}
	
	pAddPath = PYTHON_FROMSTRING(cfg->plugins_path);
	if (!pAddPath) {
		return -1;
	}

	if (PyList_Insert(pPath, 0, pAddPath) < 0) {
		retcode = -1;
	}
	Py_DECREF(pAddPath);

	return retcode;
}


/**
 *
 */
int proxenet_python_initialize_vm(plugin_t* plugin) 
{
	interpreter_t *interpreter = plugin->interpreter;
	
	/* is vm initialized ? */
	if (interpreter->ready)
		return 0;
	
#ifdef DEBUG
	xlog(LOG_DEBUG, "%s\n", "Initializing Python VM");
#endif
		
	Py_Initialize();
	if (Py_IsInitialized() == false) {
		xlog(LOG_CRITICAL, "%s\n", "Failed to initialize Python engine");
		interpreter->vm = NULL;
		interpreter->ready = false;
		return -1;
	}

	if (proxenet_python_append_path(interpreter) < 0) {
		xlog(LOG_CRITICAL, "%s\n", "Failed to append plugins directory to sys.path");
		Py_Finalize();
		interpreter->vm = NULL;
		interpreter->ready = false;
		return -1;
	}
	
	interpreter->ready = true;
	return 0;
}


/**
 *
 */
int proxenet_python_destroy_vm(plugin_t* plugin)
{
	if (! Py_IsInitialized()) {
		xlog(LOG_CRITICAL, "%s\n", "Python VM should not be uninitialized here");
		return -1;
	}
	
	if(count_plugins_by_type(_PYTHON_) == 0) {
		Py_Finalize();
	}

	plugin->interpreter->ready = false;
	return 0;
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
	bool is_request = (type==REQUEST) ? true : false;
	
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

	function_name = is_request ? CFG_REQUEST_PLUGIN_FUNCTION : CFG_RESPONSE_PLUGIN_FUNCTION;
	
	/* import module */
	module_name_len = strlen(plugin->name) + 2; 
	module_name = alloca(module_name_len);
	proxenet_xzero(module_name, module_name_len);

	if (snprintf(module_name, module_name_len, "%d%s", plugin->priority, plugin->name) < 0){
		xlog(LOG_ERROR, "snprintf failed : %s\n", strerror(errno));
		return -1;
	}
	
	pModStr = PYTHON_FROMSTRING(module_name);
	if (!pModStr) {
		PyErr_Print();
		return -1;
	}

	pMod = PyImport_Import(pModStr);
	if(!pMod) {
		xlog(LOG_ERROR, "Failed to import '%s'\n", module_name);
		PyErr_Print(); 
		Py_DECREF(pModStr);
		return -1;
	}

	Py_DECREF(pModStr);

#ifdef DEBUG
	xlog(LOG_DEBUG, "Importing '%s.%s'\n", module_name, function_name);
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
char* proxenet_python_execute_function(PyObject* pFuncRef, long rid, char* request_str, size_t* request_size)
{
	PyObject *pArgs, *pResult;
	char *buffer, *result;
	int ret;
	Py_ssize_t len;

	result = buffer = NULL;
	len = -1;

#if _PYTHON_MAJOR_ == 3
	pArgs = Py_BuildValue("iy", rid, request_str);
#else
	pArgs = Py_BuildValue("is", rid, request_str);
#endif
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


	if (PYTHON_CHECK(pResult)) {
		ret = PYTHON_ASSTRINGANDSIZE(pResult, &buffer, &len);
		if (ret < 0) {
			PyErr_Print();
			
		} else {
			result = (char*) proxenet_xmalloc(len+1);
			result = memcpy(result, buffer, len);
			*request_size = len;
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
void proxenet_python_lock_vm(interpreter_t *interpreter)
{
	pthread_mutex_lock(&interpreter->mutex);

}


/**
 *
 */
void proxenet_python_unlock_vm(interpreter_t *interpreter)
{
	pthread_mutex_unlock(&interpreter->mutex);
}


/**
 * 
 */
char* proxenet_python_plugin(plugin_t* plugin, long rid, char* request, size_t* request_size, int type)
{	
	char *dst_buf = NULL;
	PyObject *pFunc = NULL;
	bool is_request = (type==REQUEST) ? true : false;
	interpreter_t *interpreter = plugin->interpreter;
	
	if (!interpreter->ready)
		return request;

	proxenet_python_lock_vm(interpreter);
       
	if (is_request)
		pFunc = (PyObject*) plugin->pre_function;
	else
		pFunc = (PyObject*) plugin->post_function;

	dst_buf = proxenet_python_execute_function(pFunc, rid, request, request_size);
	if (!dst_buf) {
		xlog(LOG_ERROR,
		     "[%s] Error while executing plugin on %s\n",
		     plugin->name,
		     is_request ? "request" : "response");
		
		dst_buf = request;
	}
	

	proxenet_python_unlock_vm(interpreter);
	
	return dst_buf;
}

#endif /* _PYTHON_PLUGIN */


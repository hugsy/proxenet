/*******************************************************************************
 *
 * Python plugin implementation
 *
 * https://mdavey.wordpress.com/2006/02/09/python-rule-for-py_newinterpreter/
 */


#ifdef _PYTHON_PLUGIN

#include <Python.h>


static PyThreadState* proxenet_main_interpreter;

/**
 *
 */
void proxenet_python_initialize_vm(plugin_t* plugin) 
{
	/* PyThreadState* pOldCtx = NULL; */
	
	if(count_initialized_plugins_by_type(PYTHON) == 0){
		/* first time ? */
		Py_Initialize();
		if (Py_IsInitialized() == FALSE) {
			xlog(LOG_CRITICAL, "Failed to initialize Python engine\n");
			plugin->interpreter = NULL;
			return;
		}
		
		proxenet_main_interpreter = PyThreadState_Get();
		
		PyEval_InitThreads();
	}
	
	/* a new interpreter does not have argv (required by many modules), we need to add one */
	char *PyArgv = "proxenet_python_plugin_engine";
	
	plugin->interpreter = (void*)Py_NewInterpreter();
	if (plugin->interpreter == NULL) {
		xlog(LOG_CRITICAL, "Failed to create new python sub-interpreter\n");
		abort();
	}
	PySys_SetArgv(1, &PyArgv);
	
	PyThreadState_Swap(proxenet_main_interpreter);
	
	/* pOldCtx = PyEval_SaveThread();  */
	Py_BEGIN_ALLOW_THREADS
		
		PyThreadState_Swap(plugin->interpreter);
	
	/* load plugin path */
	if(proxenet_python_add_plugins_path(cfg->plugins_path)<0) {
		xlog(LOG_ERROR, "Failed to extend path with '%s'\n", cfg->plugins_path);
		proxenet_python_destroy_vm(plugin);
		return;
	}
	
	/* proxenet_proxenet_current_interpreter = pOldCtx; */
	/* PyThreadState_Swap(proxenet_proxenet_current_interpreter); */
	
	Py_END_ALLOW_THREADS
		
		PyThreadState_Swap(proxenet_main_interpreter);	  
}



/**
 *
 */
void proxenet_python_destroy_vm(plugin_t* plugin)
{
	if (!Py_IsInitialized() || !PyEval_ThreadsInitialized())
		return ;
	
	PyThreadState_Swap((PyThreadState *)plugin->interpreter);
	Py_EndInterpreter((PyThreadState *)plugin->interpreter);
	
	PyThreadState_Swap(proxenet_main_interpreter);
	
	plugin->interpreter = NULL;
	
	if(count_plugins_by_type(PYTHON) == 0) {
		Py_Finalize();
	}
	
}


/**
 *
 */
boolean proxenet_python_path_already_loaded(char* module_path, PyObject* path_list)
{
	int i;
	PyObject* pListItem;
	char *pCurItem;
	boolean found = FALSE;
	
	for(i = 0; i < PyList_Size(path_list); i++) {
		pCurItem = NULL;
		pListItem = PyList_GetItem(path_list, i);
		if(PyUnicode_Check(pListItem)) {
			pCurItem = PyBytes_AsString(PyUnicode_AsEncodedString(pListItem, "utf-8", "Error"));
			
			if(strcmp(module_path, pCurItem)==0) {
				found = TRUE;
#ifdef DEBUG
				xlog(LOG_DEBUG, "'%s' already in path\n", module_path);
#endif
			}
		}
	}
	
	return found;
}


/**
 *
 */
void proxenet_python_append_list(PyObject* pList, char* element) 
{
	PyObject *pPath;
	pPath = PyUnicode_FromString(element);
	PyList_Append(pList, pPath);
	/* Py_DECREF(pPath); */
}


/**
 *
 */
int proxenet_python_add_plugins_path(char* module_path)
{
	PyObject *pDictMod; 
	PyObject *pString;
	PyObject *pSys;
	PyObject *pPathList;
	
	/* import sys */
	pDictMod = PyImport_GetModuleDict();  // import python modules dico
	pString = PyUnicode_FromString("sys");
	pSys = PyDict_GetItem(pDictMod, pString); // look for sys module 
	if(!pSys) {
		xlog(LOG_ERROR, "Module 'sys' not found !\n");
		return -1;
	}
	
	pDictMod = PyModule_GetDict(pSys);
	pPathList = PyDict_GetItemString(pDictMod, "path"); // look for path object
	if(!PyList_Check(pPathList)) {
		xlog(LOG_ERROR, "Invalid 'path' object type !\n");
		return -1;
	}
	
	/* sys.path.append(/my/path) */  
	if (proxenet_python_path_already_loaded(module_path, pPathList) != TRUE) {
		proxenet_python_append_list(pPathList, module_path);
	}
	
	/* Py_DECREF(pString); */
	/* Py_DECREF(pDictMod); */
	/* Py_DECREF(pPathList); */
	return 0;
}


/**
 *
 */
PyObject* proxenet_python_import_module(char* module_name)
{
	PyObject* pPluginName;
	
	pPluginName = PyString_FromString(module_name);
	return PyImport_Import(pPluginName);
}


/**
 *
 */
PyObject* proxenet_python_load_function(plugin_t* plugin, const char* function_name) 
{     
	char* module_name;
	size_t module_name_len;
	PyObject* pPluginMod;
	PyObject* pPluginDict;
	PyObject* pFunctionItem;
	
	module_name_len = strlen(plugin->name)-strlen(plugins_extensions_str[PYTHON]);
	module_name = (char*)xmalloc(module_name_len+1);
	memcpy(module_name, plugin->name, module_name_len);
	
	
	/* import module */
	pPluginMod = proxenet_python_import_module(module_name);
	if(pPluginMod == 0) {
		xlog(LOG_ERROR, "Failed to import '%s'\n", module_name);
		xfree(module_name);
		return NULL;
	}
	
	xfree(module_name);
	
	pPluginDict = PyModule_GetDict(pPluginMod); // get plugin module dict
	pFunctionItem = PyDict_GetItemString(pPluginDict, function_name); // get func reference
	
	/* Py_DECREF(pPluginDict); */
	
	/* success if function is callable */
	return PyCallable_Check(pFunctionItem) ? pFunctionItem : NULL;
}


/**
 *
 */
char* proxenet_python_execute_function(PyObject* pFuncRef, char* http_request)
{
	PyObject* pResult;
	char* result_buf;
	
	pResult = PyObject_CallFunction(pFuncRef, "s", http_request);
	
	if (PyString_Check(pResult)){
		char* ptr = PyString_AsString(pResult);
		result_buf = (char*)xmalloc(strlen(ptr)+1);
		memcpy(result_buf, ptr, strlen(ptr));
		
	} else {
		xlog(LOG_ERROR, "proxenet_python_execute_function: incorrect return type (not string)\n");
		result_buf = NULL;
	}
	
	/* Py_DECREF(pResult); */
	
	return result_buf;
}


/**
 * loads lib, get func ref, execute func, memcpy result
 */
char* proxenet_python_plugin(plugin_t* plugin, char* request, const char* function_name)
{
#ifdef DEBUG
	xlog(LOG_DEBUG, "[%s] start %s\n", plugin->name, function_name);
#endif
	
	char*  dst_buf = NULL;
	PyObject* pFunctionReference = NULL;
	
	if (!plugin->interpreter)
		return request;
	
	
	/* Py_BEGIN_ALLOW_THREADS; // protecting python thread   */
	
	PyThreadState_Swap(plugin->interpreter);
	
	pFunctionReference  = proxenet_python_load_function(plugin, function_name);
	if (!pFunctionReference) {
		if (cfg->verbose)
			xlog(LOG_ERROR, "[%s] Function '%s'\n", plugin->name, function_name);
		if (cfg->verbose > 1)
			PyErr_Print();
		return request;
	}
	
	dst_buf = proxenet_python_execute_function(pFunctionReference, request);
	if (!dst_buf) {
		xlog(LOG_ERROR,"[%s] Error while executing '%s'\n", plugin->name, dst_buf);
		return request;
	}
	
	/* Py_END_ALLOW_THREADS; // release lock */
	
#ifdef DEBUG
	xlog(LOG_DEBUG, "[%s] end %s\n", plugin->name, function_name);
#endif
	
	return dst_buf;
}

#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _JAVA_PLUGIN

/*******************************************************************************
 *
 * Java plugin
 *
 */

#include <jni.h>
#include <string.h>
#include <stdlib.h>

#include "core.h"
#include "utils.h"
#include "main.h"
#include "plugin.h"
#include "plugin-java.h"

#define xlog_java(t, ...) xlog(t, "["_JAVA_VERSION_"] " __VA_ARGS__)


/**
 *
 */
static int proxenet_java_get_class(JNIEnv *env, char *name, void **res)
{
        jclass jcls;

        jcls = (*env)->FindClass(env, name);
        if(!jcls){
                xlog_java(LOG_ERROR, "Java class '%s' not found\n", name);
                return -1;
        }

        *res = (void*)jcls;

#ifdef DEBUG
        xlog_java(LOG_DEBUG, "Class '%s' jcls=%#lx\n", name, jcls);
#endif

        return 0;
}


/**
 *
 */
static int proxenet_java_get_method(JNIEnv *env, jclass jcls, char* name, char *proto, void **res)
{
        jmethodID jmid;

        jmid = (*env)->GetStaticMethodID(env, jcls, name, proto);
        if(!jmid){
                xlog_java(LOG_ERROR, "Method '%s()' not found\n", name);
                return -1;
        }

#ifdef DEBUG
        xlog_java(LOG_DEBUG, "'%s()' jmid=%#lx\n", name, jmid);
#endif

        *res = (void*)jmid;

        return 0;
}


/**
 *
 */
static inline int proxenet_execute_plugin_method(plugin_t* plugin, jmethodID jmid)
{
        JavaVM* jvm;
        JNIEnv *env;
        jclass jcls;

        jvm = ((proxenet_jvm_t*)plugin->interpreter->vm)->jvm;
        jcls = (jclass)plugin->internal;

        if (!jcls || !jmid)
                return -1;

        (*jvm)->AttachCurrentThread(jvm, (void**)&env, NULL);
        (*env)->CallStaticObjectMethod(env, jcls, jmid);
        (*jvm)->DetachCurrentThread(jvm);
        return 0;

}


/**
 *
 */
static int proxenet_execute_onload_method(plugin_t* plugin)
{
        return proxenet_execute_plugin_method(plugin, plugin->onload_function);
}


/**
 *
 */
static int proxenet_execute_onleave_method(plugin_t* plugin)
{
        return proxenet_execute_plugin_method(plugin, plugin->onleave_function);
}


/**
 * Loads a compiled Java file (.class), allocates the structure, and execute the onload()
 * function.
 */
int proxenet_java_load_file(plugin_t* plugin)
{
        proxenet_jvm_t* pxnt_jvm;
        JavaVM *jvm;
        JNIEnv *env;

        if(plugin->state != INACTIVE){
#ifdef DEBUG
                if(cfg->verbose > 2)
                        xlog_java(LOG_DEBUG, "Plugin '%s' is already loaded. Skipping...\n", plugin->name);
#endif
                return 0;
        }

        pxnt_jvm = (proxenet_jvm_t*) plugin->interpreter->vm;
	jvm = (JavaVM*) pxnt_jvm->jvm;
        env = (JNIEnv*) pxnt_jvm->env;

#ifdef DEBUG
        xlog_java(LOG_DEBUG, "Trying to load Java class '%s'\n", plugin->name);
#endif

        /* check that Class can be found */
        if(proxenet_java_get_class(env, plugin->name, &plugin->internal) < 0)
                goto detach_vm;

        /* check that Methods can be found in Class */
        if(proxenet_java_get_method(env, plugin->internal, CFG_REQUEST_PLUGIN_FUNCTION, JAVA_PLUGIN_METHOD_SIGNATURE, &plugin->pre_function) < 0)
                goto detach_vm;

        if(proxenet_java_get_method(env, plugin->internal, CFG_RESPONSE_PLUGIN_FUNCTION, JAVA_PLUGIN_METHOD_SIGNATURE, &plugin->post_function) < 0)
                goto detach_vm;

        proxenet_java_get_method(env, plugin->internal, CFG_ONLOAD_PLUGIN_FUNCTION, JAVA_VOID_METHOD_SIGNATURE, &plugin->onload_function);
        proxenet_java_get_method(env, plugin->internal, CFG_ONLEAVE_PLUGIN_FUNCTION, JAVA_VOID_METHOD_SIGNATURE, &plugin->onleave_function);

        (*jvm)->DetachCurrentThread(jvm);

        if(plugin->onload_function)
                if (proxenet_execute_onload_method(plugin) < 0){
                        xlog_java(LOG_ERROR, "An error occured on %s.onload()\n", plugin->name);
                }

        return 0;

detach_vm:
        (*jvm)->DetachCurrentThread(jvm);
	return -1;
}


/**
 *
 */
int proxenet_java_initialize_vm(plugin_t* plugin)
{
        int ret;
        JavaVMInitArgs vm_args;
        JavaVMOption options;
        proxenet_jvm_t *pxnt_jvm;
        char java_classpath_option[256] = {0, };

	if (plugin->interpreter->ready)
		return 0;

        pxnt_jvm = proxenet_xmalloc(sizeof(proxenet_jvm_t));

        ret = proxenet_xsnprintf(java_classpath_option, sizeof(java_classpath_option),
                                 "-Djava.class.path=%s", cfg->plugins_path);
        if (ret < 0)
                return -1;

        options.optionString = java_classpath_option;

#if     (_JAVA_MINOR_ == 8)
        vm_args.version = JNI_VERSION_1_8;
#elif   (_JAVA_MINOR_ == 7) || (_JAVA_MINOR_ == 6)
        vm_args.version = JNI_VERSION_1_6;
#endif

        vm_args.nOptions = 1;
        vm_args.options = &options;
        vm_args.ignoreUnrecognized = JNI_TRUE;

        ret = JNI_CreateJavaVM(&pxnt_jvm->jvm, (void**)&pxnt_jvm->env, &vm_args);
        if (ret != JNI_OK) {
                xlog_java(LOG_ERROR, "Failed to initialize JVM (%d)\n", ret);
                plugin->interpreter->ready = false;
                return -1;
        }

#ifdef DEBUG
        xlog_java(LOG_DEBUG, "JVM created jvm=%#lx env=%#lx\n", pxnt_jvm->jvm, pxnt_jvm->env);
#endif

	plugin->interpreter->vm = (void*)pxnt_jvm;
	plugin->interpreter->ready = true;

	return 0;
}



/**
 * Disable the Java plugin, invoke onleave() function (if any), and deallocate
 * plugin structures.
 */
int proxenet_java_destroy_plugin(plugin_t* plugin)
{
        proxenet_plugin_set_state(plugin, INACTIVE);

        if (plugin->onleave_function)
                if (proxenet_execute_onleave_method(plugin) < 0){
                        xlog_java(LOG_ERROR, "An error occured on %s.onleave()\n", plugin->name);
                }

        plugin->pre_function = NULL;
        plugin->post_function = NULL;
        plugin->onload_function = NULL;
        plugin->onleave_function = NULL;

        return 0;
}


/**
 *
 */
int proxenet_java_destroy_vm(interpreter_t* interpreter)
{
        proxenet_jvm_t *pxnt_jvm;
        JavaVM *jvm;

        pxnt_jvm = (proxenet_jvm_t*) interpreter->vm;
	jvm = (JavaVM*) pxnt_jvm->jvm;

        (*jvm)->DestroyJavaVM(jvm);

        proxenet_xfree(interpreter->vm);
	interpreter->ready = false;
        interpreter->vm = NULL;

        return 0;
}


/**
 *
 */
static char* proxenet_java_execute_plugin_function(plugin_t* plugin, request_t *request)
{
	char *buf, *uri, *meth;
        proxenet_jvm_t* pxnt_jvm;

	JavaVM* jvm;
        JNIEnv *env;
        jclass jcls;
        jmethodID jmid;

        jint jrid;
        jbyteArray jreq;
        jstring juri;
        jbyteArray jret;
        jsize jretlen;
        jbyte *jret2;
        jboolean is_copy;

        buf = NULL;

	uri = request->http_infos.uri;
	if (!uri)
		return NULL;


        /* get method id inside the JVM */
        pxnt_jvm = (proxenet_jvm_t*)plugin->interpreter->vm;
	jvm = pxnt_jvm->jvm;
        (*jvm)->AttachCurrentThread(jvm, (void**)&env, NULL);

        if (request->type==REQUEST){
                meth = CFG_REQUEST_PLUGIN_FUNCTION;
                jcls = (jclass)plugin->internal;
                jmid = (jmethodID)plugin->pre_function;
        } else {
                meth = CFG_RESPONSE_PLUGIN_FUNCTION;
                jcls = (jclass)plugin->internal;
                jmid = (jmethodID)plugin->post_function;
        }

#ifdef DEBUG
        xlog_java(LOG_DEBUG, "'%s:%s' -> jvm=%#lx env=%#lx jcls=%#lx jmid=%#lx\n",
                  plugin->name, meth, jvm, env, jcls, jmid);
#endif

        /* prepare the arguments */
        jrid = request->id;
        jreq = (*env)->NewByteArray(env, request->size);
        (*env)->SetByteArrayRegion(env, jreq, 0, request->size, (jbyte*)request->data);
        juri = (*env)->NewStringUTF(env, uri);

#ifdef DEBUG
        xlog_java(LOG_DEBUG, "'%s:%s' -> jrid=%#lx jreq=%#lx juri=%#lx\n",
                  plugin->name, meth, jrid, jreq, juri);
#endif

        /* call the method id */
        jret = (*env)->CallStaticObjectMethod(env, jcls, jmid, jrid, jreq, juri);
        if(!jret){
                xlog_java(LOG_ERROR, "An error occured when invoking '%s.%s()'\n", plugin->name, meth);
                goto end;
        }

        jretlen = (*env)->GetArrayLength(env, jret);
        jret2 = (*env)->GetByteArrayElements(env, jret, &is_copy);


        /* treat the result */
	buf = proxenet_xstrdup((char*)jret2, jretlen);
	if (!buf)
		goto end;

	request->size = jretlen;

end:
        (*jvm)->DetachCurrentThread(jvm);

	return buf;
}

/**
 *
 */
static inline void proxenet_java_lock_vm(interpreter_t *interpreter)
{
	pthread_mutex_lock(&interpreter->mutex);
}


/**
 *
 */
static inline void proxenet_java_unlock_vm(interpreter_t *interpreter)
{
	pthread_mutex_unlock(&interpreter->mutex);
}


/**
 *
 */
char* proxenet_java_plugin(plugin_t* plugin, request_t *request)
{
	char* buf = NULL;
	interpreter_t *interpreter = plugin->interpreter;

	proxenet_java_lock_vm(interpreter);
	buf = proxenet_java_execute_plugin_function(plugin, request);
	proxenet_java_unlock_vm(interpreter);

	return buf;
}

#endif /* _JAVA_PLUGIN */

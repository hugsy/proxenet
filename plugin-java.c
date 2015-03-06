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


/**
 *
 */
int proxenet_java_load_file(plugin_t* plugin)
{
        proxenet_jvm_t* pxnt_jvm;
        JavaVM *jvm;
        JNIEnv *env;
        jclass jcls;
        jmethodID jmid;

        if(plugin->state != INACTIVE){
#ifdef DEBUG
                if(cfg->verbose > 2)
                        xlog(LOG_DEBUG, "Plugin '%s' is already loaded. Skipping...\n", plugin->name);
#endif
                return 0;
        }

        pxnt_jvm = (proxenet_jvm_t*) plugin->interpreter->vm;
	jvm = (JavaVM*) pxnt_jvm->jvm;
        env = (JNIEnv*) pxnt_jvm->env;

#ifdef DEBUG
        xlog(LOG_DEBUG, "Trying to load Java class '%s'\n", plugin->name);
#endif

        /* check that Class can be found */
        jcls = (*env)->FindClass(env, plugin->name);
        if(!jcls){
                xlog(LOG_ERROR, "Java class '%s' not found\n", plugin->name);
                return -1;
        }

        plugin->class = (void*)jcls;

#ifdef DEBUG
        xlog(LOG_DEBUG, "Class '%s' jcls=%#lx\n", plugin->name, jcls);
#endif

        /* check that Methods can be found in Class */
        /* check request hook */
        jmid = (*env)->GetStaticMethodID(env, jcls, CFG_REQUEST_PLUGIN_FUNCTION, JAVA_METHOD_PROTOTYPE);
        if(!jmid){
                xlog(LOG_ERROR, "Method '%s.%s()' not found\n", plugin->name, CFG_REQUEST_PLUGIN_FUNCTION);
                return -1;
        }
#ifdef DEBUG
        xlog(LOG_DEBUG, "'%s.%s()' jmid=%#lx\n", plugin->name, CFG_REQUEST_PLUGIN_FUNCTION, jmid);
#endif
        plugin->pre_function = (void*)jmid;

        /* check response hook */
        jmid = (*env)->GetStaticMethodID(env, jcls, CFG_RESPONSE_PLUGIN_FUNCTION, JAVA_METHOD_PROTOTYPE);
        if(!jmid){
                xlog(LOG_ERROR, "Method '%s.%s()' not found\n", plugin->name, CFG_RESPONSE_PLUGIN_FUNCTION);
                return -1;
        }
#ifdef DEBUG
        xlog(LOG_DEBUG, "'%s.%s()' jmid=%#lx\n", plugin->name, CFG_RESPONSE_PLUGIN_FUNCTION, jmid);
#endif
        plugin->post_function = (void*)jmid;

        (*jvm)->DetachCurrentThread(jvm);

	return 0;
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

	if (plugin->interpreter->ready)
		return 0;

        pxnt_jvm = proxenet_xmalloc(sizeof(proxenet_jvm_t));

        options.optionString = "-Djava.class.path="JAVA_CLASSPATH;

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
                xlog(LOG_ERROR, "Failed to initialize JVM (%d)\n", ret);
                plugin->interpreter->ready = false;
                return -1;
        }

#ifdef DEBUG
        xlog(LOG_DEBUG, "JVM created jvm=%#lx env=%#lx\n", pxnt_jvm->jvm, pxnt_jvm->env);
#endif

	plugin->interpreter->vm = (void*)pxnt_jvm;
	plugin->interpreter->ready = true;

	return 0;
}



/**
 *
 */
int proxenet_java_destroy_plugin(plugin_t* plugin)
{
        plugin->state = INACTIVE;
        plugin->pre_function = NULL;
        plugin->post_function = NULL;

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
static char* proxenet_java_execute_function(plugin_t* plugin, request_t *request)
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

	uri = request->uri;
	if (!uri)
		return NULL;


        /* get method id inside the JVM */
        pxnt_jvm = (proxenet_jvm_t*)plugin->interpreter->vm;
	jvm = pxnt_jvm->jvm;
        (*jvm)->AttachCurrentThread(jvm, (void**)&env, NULL);

        if (request->type==REQUEST){
                meth = CFG_REQUEST_PLUGIN_FUNCTION;
                jcls = (jclass)plugin->class;
                jmid = (jmethodID)plugin->pre_function;
        } else {
                meth = CFG_RESPONSE_PLUGIN_FUNCTION;
                jcls = (jclass)plugin->class;
                jmid = (jmethodID)plugin->post_function;
        }

#ifdef DEBUG
        xlog(LOG_DEBUG, "'%s:%s' -> jvm=%#lx env=%#lx jcls=%#lx jmid=%#lx\n",
             plugin->name, meth, jvm, env, jcls, jmid);
#endif

        /* prepare the arguments */
        jrid = request->id;
        jreq = (*env)->NewByteArray(env, request->size);
        (*env)->SetByteArrayRegion(env, jreq, 0, request->size, (jbyte*)request->data);
        juri = (*env)->NewStringUTF(env, uri);

#ifdef DEBUG
        xlog(LOG_DEBUG, "'%s:%s' -> jrid=%#lx jreq=%#lx juri=%#lx\n",
             plugin->name, meth, jrid, jreq, juri);
#endif

        /* call the method id */
        jret = (*env)->CallStaticObjectMethod(env, jcls, jmid, jrid, jreq, juri);
        if(!jret){
                xlog(LOG_ERROR, "An error occured when invoking '%s.%s()'\n", plugin->name, meth);
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
static void proxenet_java_lock_vm(interpreter_t *interpreter)
{
	pthread_mutex_lock(&interpreter->mutex);
}


/**
 *
 */
static void proxenet_java_unlock_vm(interpreter_t *interpreter)
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
	buf = proxenet_java_execute_function(plugin, request);
	proxenet_java_unlock_vm(interpreter);

	return buf;
}

#endif /* _JAVA_PLUGIN */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _JAVA_PLUGIN

#include <jni.h>

#define JAVA_METHOD_PROTOTYPE   "(I[BLjava/lang/String;)[B"          // Signature extracted using `javap -s -p <ClassName>`

// TODO use cmake
#ifndef JAVA_CLASSPATH
#define JAVA_CLASSPATH "/home/hugsy/code/proxenet/proxenet-plugins"
#endif

typedef struct {
                JavaVM* jvm;
                JNIEnv *env;
                jclass jcls;
} proxenet_jvm_t;

int 	proxenet_java_initialize_vm(plugin_t*);
int	proxenet_java_destroy_vm(plugin_t*);
int 	proxenet_java_load_file(plugin_t*);
char* 	proxenet_java_plugin(plugin_t*, request_t*);

#endif /* _JAVA_PLUGIN */

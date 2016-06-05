#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _JAVA_PLUGIN

#include <jni.h>

#define JAVA_PLUGIN_METHOD_SIGNATURE   "(I[BLjava/lang/String;)[B"          // Signature extracted using `javap -s -p <ClassName>`
#define JAVA_VOID_METHOD_SIGNATURE   "()V"

typedef struct {
                JavaVM* jvm;
                JNIEnv *env;
                jclass jcls;
} proxenet_jvm_t;

int 	proxenet_java_initialize_vm(plugin_t*);
int     proxenet_java_destroy_plugin(plugin_t*);
int	proxenet_java_destroy_vm(interpreter_t*);
int 	proxenet_java_load_file(plugin_t*);
char* 	proxenet_java_plugin(plugin_t*, request_t*);

#endif /* _JAVA_PLUGIN */

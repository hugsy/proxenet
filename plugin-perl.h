#ifdef _PERL_PLUGIN

#include "plugin.h"

int proxenet_perl_initialize_vm(plugin_t*);
int proxenet_perl_destroy_vm(plugin_t*);
char* proxenet_perl_execute_function(plugin_t*, const char*, long, char*, size_t*);
void proxenet_perl_lock_vm(interpreter_t*);
void proxenet_perl_unlock_vm(interpreter_t*);
char* proxenet_perl_plugin(plugin_t*, long, char*, size_t*, int);
void proxenet_perl_preinitialisation(int argc, char** argv, char** envp);
void proxenet_perl_postdeletion();

#endif

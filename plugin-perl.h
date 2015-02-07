#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _PERL_PLUGIN

#include "plugin.h"

int proxenet_perl_initialize_vm(plugin_t*);
int proxenet_perl_destroy_vm(plugin_t*);
char* proxenet_perl_plugin(plugin_t*, request_t*);
void proxenet_perl_preinitialisation(int, char**, char**);
void proxenet_perl_postdeletion();

#endif

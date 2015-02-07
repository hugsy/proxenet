#ifdef _TCL_PLUGIN


int 	proxenet_tcl_initialize_vm(plugin_t*);
int	proxenet_tcl_destroy_vm(plugin_t*);
int 	proxenet_tcl_load_file(plugin_t*);
char* 	proxenet_tcl_plugin(plugin_t*, request_t*);

#endif /* HAVE_LIBTCL */

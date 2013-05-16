#ifdef _PERL_PLUGIN

/*******************************************************************************
 *
 * Perl plugin 
 *
 */


#include <EXTERN.h> 
#include <perl.h>
#include <string.h>
#include <alloca.h>

#include "plugin.h"
#include "plugin-perl.h"
#include "utils.h"
#include "main.h"


static PerlInterpreter *my_perl;


/**
 *
 */
int proxenet_perl_load_file(plugin_t* plugin)
{
	char *args[2], *pathname;
	size_t pathlen;
	
	pathlen = strlen(cfg->plugins_path) + 1 + strlen(plugin->filename) + 1;
	pathname = (char*) alloca(pathlen+1);
	proxenet_xzero(pathname, pathlen+1);
	snprintf(pathname, pathlen, "%s/%s", cfg->plugins_path, plugin->filename);

#ifdef DEBUG
	xlog(LOG_DEBUG, "[Perl] Loading '%s'\n", pathname);
#endif
		
	args[0] = "";	
	args[1] = pathname;
	perl_parse(my_perl, NULL, 2, args, NULL);

	return 0;
}


/**
 *
 */
int proxenet_perl_initialize_vm(plugin_t* plugin)
{
	interpreter_t *interpreter;
	interpreter = plugin->interpreter;
	
	/* checks */
	if (!interpreter->ready){	

#ifdef DEBUG
		xlog(LOG_DEBUG, "[Perl] %s\n", "Initializing VM");
#endif
	
		/* vm init */
		my_perl = perl_alloc();
		perl_construct(my_perl);
		PL_exit_flags |= PERL_EXIT_DESTRUCT_END;

		if (!my_perl) {
			xlog(LOG_ERROR, "[Perl ]%s\n", "failed init-ing vm");
			return -1;
		}

		interpreter->vm = (void*) my_perl;
		interpreter->ready = true;
	}

	proxenet_perl_load_file(plugin);
	
	return 0;
}


/**
 *
 */
int proxenet_perl_destroy_vm(plugin_t* plugin)
{
	
	if (!plugin->interpreter->ready)
		return -1;
	
	perl_destruct(my_perl);
        perl_free(my_perl);
	PERL_SYS_TERM();

	plugin->interpreter->vm = NULL;
	plugin->interpreter->ready = false;
	return 0;
}


/**
 *
 */
char* proxenet_perl_execute_function(plugin_t* plugin, const char* fname, long rid, char* request_str)
{
	dSP;
	char *res, *data;
	int nb_res;
	size_t len;

	res = data = NULL;
	len = strlen(request_str);

	ENTER;
	SAVETMPS;

	PUSHMARK(SP);
	XPUSHs(sv_2mortal(newSVuv(rid)));
        XPUSHs(sv_2mortal(newSVpv(request_str, 0)));
        PUTBACK;

	nb_res = call_pv(fname, G_SCALAR);
	
	SPAGAIN;

	if (nb_res != 1) {
		xlog(LOG_ERROR, "[Perl] Invalid number of response returned (got %d, expected 1)\n", nb_res);
		data = NULL;
		
	} else {
		res = (char*) POPp;
		len = strlen(res);
		data = (char*) proxenet_xmalloc(len+1);
		memcpy(data, res, len);
	}
	
	PUTBACK;
        FREETMPS;
        LEAVE;
	
	return data;
}


/**
 *
 */
void proxenet_perl_lock_vm(interpreter_t *interpreter)
{
	pthread_mutex_lock(&interpreter->mutex);
}


/**
 *
 */
void proxenet_perl_unlock_vm(interpreter_t *interpreter)
{
	pthread_mutex_unlock(&interpreter->mutex);
}


/**
 * 
 */
char* proxenet_perl_plugin(plugin_t* plugin, long rid, char* request, int type)
{
	interpreter_t *interpreter; 
	const char *function_name;
	char *buf;

	interpreter = plugin->interpreter;
	if (!interpreter->ready)
		return NULL;

	if (type == REQUEST)
		function_name = CFG_REQUEST_PLUGIN_FUNCTION;
	else 
		function_name = CFG_RESPONSE_PLUGIN_FUNCTION;

	proxenet_perl_lock_vm(interpreter);
	buf = proxenet_perl_execute_function(plugin, function_name, rid, request);
	proxenet_perl_unlock_vm(interpreter);

	return buf;
}

#endif /* _PERL_PLUGIN */

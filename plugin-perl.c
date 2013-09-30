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

#include "core.h"
#include "plugin.h"
#include "plugin-perl.h"
#include "utils.h"
#include "main.h"


static PerlInterpreter *my_perl;


/**
 *
 */
static int proxenet_perl_load_file(plugin_t* plugin)
{
	char *pathname = NULL;
	size_t pathlen = 0;
	SV* sv = NULL;
	int nb_res = -1;
	SV* package_sv = NULL;
	char *required  = NULL;
	char *package_name = NULL;
	size_t package_len, len = 0;
	int ret = -1;
	
	
	pathlen = strlen(cfg->plugins_path) + 1 + strlen(plugin->filename) + 1;
	pathname = (char*) alloca(pathlen+1);
	proxenet_xzero(pathname, pathlen+1);
	snprintf(pathname, pathlen, "%s/%s", cfg->plugins_path, plugin->filename);
	
#ifdef DEBUG
	xlog(LOG_DEBUG, "[Perl] Loading '%s'\n", pathname);
#endif
	
	/* Load the file through perl's require mechanism */
	dSP;
	ENTER;
	SAVETMPS;
	
	PUSHMARK(SP);
	PUTBACK;
	
	sv = newSVpvf("$package = require q%c%s%c", 0, pathname, 0);
	nb_res = eval_sv(sv_2mortal(sv), G_EVAL);
	
	if (nb_res != 1) {
		xlog(LOG_ERROR, 
		     "[Perl] Invalid number of response returned while loading '%s' (got %d, expected 1)\n",
		     pathname,
		     nb_res);
		
	} else if (SvTRUE(ERRSV)) {
		xlog(LOG_ERROR, "[Perl] Eval error for '%s': %s\n", pathname, SvPV_nolen(ERRSV));
		
	} else {
		/* Get the package name from the package (which should follow the convention...) */
		package_sv = get_sv("package", 0);
		
		/* Check if the SV* stores a string */
		if (!SvPOK(package_sv)) {
			xlog(LOG_ERROR, "[Perl] Invalid convention for '%s': the package should return a string\n", pathname);
		} else {
			
			required = (char*) SvPV_nolen(package_sv);
			package_len = strlen(required);
			package_name = (char*) alloca(package_len+1);
			proxenet_xzero(package_name, package_len+1);
			
			memcpy(package_name, required, package_len);
			
#ifdef DEBUG
			xlog(LOG_DEBUG, "[Perl] Package of name '%s' loaded\n", package_name);
#endif
			
			/* Save the functions' full name to call them later */
			len = package_len + 2 + strlen(CFG_REQUEST_PLUGIN_FUNCTION);
			plugin->pre_function = proxenet_xmalloc(len + 1);
			snprintf(plugin->pre_function, len+1, "%s::%s", package_name, CFG_REQUEST_PLUGIN_FUNCTION);
			
			len = package_len + 2 + strlen(CFG_RESPONSE_PLUGIN_FUNCTION);
			plugin->post_function = proxenet_xmalloc(len + 1);
			snprintf(plugin->post_function, len+1, "%s::%s", package_name, CFG_RESPONSE_PLUGIN_FUNCTION);
			
			ret = 0;
		}
	}
	
	SPAGAIN;
	
	PUTBACK;
	FREETMPS;
	LEAVE;
	
	return ret;
}


/**
 *
 */
int proxenet_perl_initialize_vm(plugin_t* plugin)
{
	interpreter_t *interpreter;
	interpreter = plugin->interpreter;
	
	/* In order to perl_parse nothing */
	char *args[2] = {
		"",
		"/dev/null"
	};
	
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
			xlog(LOG_ERROR, "[Perl] %s\n", "failed init-ing vm");
			return -1;
		}

		interpreter->vm = (void*) my_perl;
		interpreter->ready = true;
		
		perl_parse(my_perl, NULL, 2, args, (char **)NULL);
	}

	return proxenet_perl_load_file(plugin);
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

	plugin->interpreter->vm = NULL;
	plugin->interpreter->ready = false;
	return 0;
}


/**
 *
 */
static char* proxenet_perl_execute_function(plugin_t* plugin, const char* fname, long rid, char* request_str, size_t* request_size)
{
	dSP;
	char *res, *data;
	int nb_res;
	size_t len;
	SV* sv = NULL;

	res = data = NULL;

	ENTER;
	SAVETMPS;

	PUSHMARK(SP);
	XPUSHs(sv_2mortal(newSVuv(rid)));
	XPUSHs(sv_2mortal(newSVpvn(request_str, *request_size)));
	PUTBACK;

	nb_res = call_pv(fname, G_SCALAR);
	
	SPAGAIN;

	if (nb_res != 1) {
		xlog(LOG_ERROR, "[Perl] Invalid number of response returned (got %d, expected 1)\n", nb_res);
		data = NULL;
		
	} else {
		sv = POPs;
		res = SvPV(sv, len);
		data = (char*) proxenet_xmalloc(len+1);
		memcpy(data, res, len);
		*request_size = len;
	}
	
	PUTBACK;
	FREETMPS;
	LEAVE;
	
	return data;
}


/**
 *
 */
static void proxenet_perl_lock_vm(interpreter_t *interpreter)
{
	pthread_mutex_lock(&interpreter->mutex);
}


/**
 *
 */
static void proxenet_perl_unlock_vm(interpreter_t *interpreter)
{
	pthread_mutex_unlock(&interpreter->mutex);
}


/**
 * 
 */
char* proxenet_perl_plugin(plugin_t* plugin, request_t* request)
{
	interpreter_t *interpreter; 
	const char *function_name;
	char *buf;

	interpreter = plugin->interpreter;
	if (!interpreter->ready)
		return NULL;

	if (request->type == REQUEST)
		function_name = plugin->pre_function;
	else 
		function_name = plugin->post_function;

	proxenet_perl_lock_vm(interpreter);
	buf = proxenet_perl_execute_function(plugin, function_name, request->id, request->data, &request->size);
	proxenet_perl_unlock_vm(interpreter);

	return buf;
}


/**
 * 
 */
void proxenet_perl_preinitialisation(int argc, char** argv, char** envp)
{
	PERL_SYS_INIT3(&argc, &argv, &envp);
}


/**
 * 
 */
void proxenet_perl_postdeletion()
{
	PERL_SYS_TERM();
}


#endif /* _PERL_PLUGIN */

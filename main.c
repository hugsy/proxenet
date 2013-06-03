#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h> 
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#include "core.h"
#include "utils.h"
#include "main.h"
#include "socket.h"



/**
 * 
 */
void version(bool end)
{
	printf("%s v%.2f\n", PROGNAME, VERSION);
	if (end) {
		proxenet_free_config();
		exit(0);
	}
}


/**
 *
 */
void usage(int retcode)
{
	FILE* fd;
	fd = (retcode == 0) ? stdout : stderr;
	
	fprintf(fd,
		"\nSYNTAX :\n"
		"\t%s [OPTIONS+]\n"
		"\nOPTIONS:\n"
		"\t-t, --nb-threads=N\t\t\tNumber of threads (default: %d)\n"
		"\t-b, --lbind=bindaddr\t\t\tBind local address (default: %s)\n"
		"\t-p, --lport=N\t\t\t\tBind local port file (default: %s)\n"
		"\t-l, --logfile=/path/to/logfile\t\tLog actions in file\n"
		"\t-x, --plugins=/path/to/plugins/dir\tSpecify plugins directory (default: %s)\n"
		"\t-X, --proxy-host=proxyhost\t\t\t\tForward to proxy\n"
		"\t-P  --proxy-port=proxyport\t\t\t\tSpecify port for proxy (default: %s)\n"
		"\t-k, --key=/path/to/ssl.key\t\tSpecify SSL key to use (default: %s)\n"
		"\t-c, --cert=/path/to/ssl.crt\t\tSpecify SSL cert to use (default: %s)\n"
		"\t-v, --verbose\t\t\t\tIncrease verbosity (default: 0)\n"
		"\t-n, --no-color\t\t\t\tDisable colored output\n"
		"\t-4, \t\t\t\t\tIPv4 only (default)\n"
		"\t-6, \t\t\t\t\tIPv6 only (default: IPv4)\n"
		"\t-h, --help\t\t\t\tShow help\n"
		"\t-V, --version\t\t\t\tShow version\n"
		,
		PROGNAME,
		CFG_DEFAULT_NB_THREAD,
		CFG_DEFAULT_BIND_ADDR,
		CFG_DEFAULT_PORT,
		CFG_DEFAULT_PLUGINS_PATH,
		CFG_DEFAULT_PROXY_PORT,
		CFG_DEFAULT_SSL_KEYFILE,
		CFG_DEFAULT_SSL_CERTFILE);
	
	exit(retcode);
}


/**
 *
 */
void help(char* argv0)
{
	const char* plugin_name;
	const char* plugin_ext;
	int i;
	
	version(false);
	printf("Written by %s\n"
	       "Released under: %s\n\n"
	       "Compiled with support for :\n",
	       AUTHOR, LICENSE);

	plugin_name = supported_plugins_str[0];
	plugin_ext  = plugins_extensions_str[0];
	
	for(i=1; plugin_name; i++) {
		printf("\t[+] 0x%.2x   %-10s (%s)\n",
		       i,
		       plugin_name,
		       plugin_ext);
		plugin_name = supported_plugins_str[i];
		plugin_ext  = plugins_extensions_str[i];
	}
	
	usage(EXIT_SUCCESS);
}


/**
 *
 */
bool parse_options (int argc, char** argv)
{
	int curopt, curopt_idx;
	char *path, *keyfile, *certfile;
	char *proxy_host, *proxy_port;

	proxy_host = proxy_port = NULL;
	
	const struct option long_opts[] = {
		{ "help",       0, 0, 'h' },
		{ "verbose",    0, 0, 'v' },
		{ "nb-threads", 1, 0, 't' },
		{ "lbind",      1, 0, 'b' },
		{ "lport",      1, 0, 'p' },
		{ "logfile",    1, 0, 'l' },
		{ "certfile",   1, 0, 'c' },
		{ "keyfile",    1, 0, 'k' },
		{ "plugins",    1, 0, 'x' },
		{ "proxy-host", 1, 0, 'X' },
		{ "proxy-port", 1, 0, 'P' },
		{ "no-color",   0, 0, 'n' },
		{ "version",    0, 0, 'V' },
		{ 0, 0, 0, 0 } 
	};
	
	cfg->iface		= CFG_DEFAULT_BIND_ADDR;
	cfg->port		= CFG_DEFAULT_PORT;
	cfg->logfile_fd		= CFG_DEFAULT_OUTPUT;
	cfg->nb_threads		= CFG_DEFAULT_NB_THREAD;
	cfg->use_color		= true;
	cfg->ip_version		= CFG_DEFAULT_IP_VERSION;
	cfg->try_exit		= 0;
	cfg->try_exit_max	= CFG_DEFAULT_TRY_EXIT_MAX;
	
	path			= CFG_DEFAULT_PLUGINS_PATH;
	keyfile			= CFG_DEFAULT_SSL_KEYFILE;
	certfile		= CFG_DEFAULT_SSL_CERTFILE;

	/* parse command line arguments */
	while (1) {
		curopt = -1;
		curopt_idx = 0;
		curopt = getopt_long (argc,argv,"n46Vhvb:p:l:t:c:k:x:X:P:",long_opts, &curopt_idx);
		if (curopt == -1) break;
		
		switch (curopt) {
			case 'v': cfg->verbose++; break;
			case 'b': cfg->iface = optarg; break;
			case 'p': cfg->port = optarg; break;
			case 'l': cfg->logfile = optarg; break;
			case 't': cfg->nb_threads = (unsigned short)atoi(optarg); break;
			case 'X':
				proxy_host = optarg;
				proxy_port = CFG_DEFAULT_PROXY_PORT;
				break;
			case 'P': proxy_port = optarg; break;
			case 'c': certfile = optarg; break;
			case 'k': keyfile = optarg; break;	   
			case 'h': help(argv[0]); break;
			case 'V': version(true); break;
			case 'n': cfg->use_color = false; break;
			case '4': cfg->ip_version = AF_INET; break;
			case '6': cfg->ip_version = AF_INET6; break;
			case 'x': path = optarg; break;
			case '?':
			default:
				usage (EXIT_FAILURE);
		}
		curopt_idx = 0;
	}

	/* validate command line arguments */
	if(cfg->logfile && is_readable_file(cfg->logfile)) {
		cfg->logfile_fd = fopen(cfg->logfile, "a");
		if(!cfg->logfile_fd) {
			fprintf(stderr, "[-] Failed to open '%s': %s\n", cfg->logfile, strerror(errno));
			return false;
		}
	}
	
	if (cfg->nb_threads > MAX_THREADS) {
		fprintf(stderr, "Too many threads. Setting to default.\n");
		cfg->nb_threads = CFG_DEFAULT_NB_THREAD;
	}
	
	if (path && is_valid_path(path))
		cfg->plugins_path = realpath(path, NULL);
	else {
		xlog(LOG_CRITICAL, "%s\n", "Invalid plugins path provided");
		return false;
	}

	if ( is_readable_file(certfile) )
		cfg->certfile = realpath(certfile, NULL);
	else {
		xlog(LOG_CRITICAL, "Failed to read certificate '%s'\n", cfg->certfile);
		return false;
	}

	if ( is_readable_file(keyfile) )
		cfg->keyfile = realpath(keyfile, NULL);
	else {
		xlog(LOG_CRITICAL, "Failed to read private key '%s'\n", cfg->keyfile);
		return false;
	}

	if ( proxy_port && !proxy_host) {
		xlog(LOG_CRITICAL, "%s\n", "Cannot use proxy-port with proxy-host");
		return false;
	}

	if (proxy_host) {
		cfg->proxy.host = strdup(proxy_host);
		cfg->proxy.port = strdup(proxy_port);
	}
	
	return true;
}


/**
 *
 */
int proxenet_init_config(int argc, char** argv)
{
	cfg = &current_config;
	proxenet_xzero(cfg, sizeof(conf_t));
	
	if (parse_options(argc, argv) == false) {
		xlog(LOG_ERROR, "%s\n", "Failed to parse arguments");
		return -1;
	}

	return 0;
}


/**
 *
 */
void proxenet_free_config()
{
	proxenet_xfree(cfg->plugins_path);
	proxenet_xfree(cfg->certfile);
	proxenet_xfree(cfg->keyfile);
	
	if(cfg->logfile_fd)
		fclose(cfg->logfile_fd);

	if (cfg->proxy.host) {
		proxenet_xfree(cfg->proxy.host);
		proxenet_xfree(cfg->proxy.port);
	}
	
}


/**
 *
 */
int main (int argc, char **argv, char** envp)
{
	int retcode = -1;
	
	/* init semaphore for unified display */
	sem_init(&tty_semaphore, 0, 1);
	
	/* get configuration */
	retcode = proxenet_init_config(argc, argv);
	if (retcode)		
		goto end;
	
	
#ifdef _PERL_PLUGIN
	/* perform plugin pre-initialisation -- currently done only for Perl */
	proxenet_init_once_plugins(argc, argv, envp);
#endif

	/* proxenet starts here  */
	retcode = proxenet_start(); 


#ifdef _PERL_PLUGIN
	/* perform plugin post-deletion */
	proxenet_delete_once_plugins();
#endif

end:

	/* proxenet ends here */
	proxenet_free_config();
	
	return (retcode == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}



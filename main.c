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
	printf("%s v%s\n", PROGNAME, VERSION);
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
		"\n"RED"SYNTAX:"NOCOLOR"\n"
		"\t%s "GREEN"[OPTIONS+]"NOCOLOR"\n"
		"\n"RED"OPTIONS:"NOCOLOR"\n"
		"\t-t, --nb-threads=N\t\t\tNumber of threads (default: %d)\n"
		"\t-b, --lbind=bindaddr\t\t\tBind local address (default: %s)\n"
		"\t-p, --lport=N\t\t\t\tBind local port file (default: %s)\n"
		"\t-l, --logfile=/path/to/logfile\t\tLog actions in file\n"
		"\t-x, --plugins=/path/to/plugins/dir\tSpecify plugins directory (default: %s)\n"
		"\t-X, --proxy-host=proxyhost\t\tForward to proxy\n"
		"\t-P  --proxy-port=proxyport\t\tSpecify port for proxy (default: %s)\n"
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

	i = 0;
	while (true) {
		plugin_name = supported_plugins_str[i];
		plugin_ext  = plugins_extensions_str[i];

		if (!plugin_name || !plugin_ext)
			break;

		printf("\t[+] 0x%.2x   "GREEN"%-10s"NOCOLOR" (%s)\n", i, plugin_name, plugin_ext);
		i++;
	}

	if (i==0)
		printf("\t[-] "RED"No support for plugin"NOCOLOR"\n");

#ifdef DEBUG
	printf("\t[+] "BLUE"Proxenet DEBUG symbols"NOCOLOR"\n");
#endif

#ifdef DEBUG_SSL
	printf("\t[+] "BLUE"PolarSSL DEBUG symbols"NOCOLOR"\n");
#endif

	usage(EXIT_SUCCESS);
}


/**
 * Checks command line arguments once and for all. Handles (de)allocation for config_t structure
 *
 * @return 0 if successfully parsed and allocated
 * @return -1
 */
int parse_options (int argc, char** argv)
{
	int curopt, curopt_idx;
	char *logfile, *plugin_path, *keyfile, *certfile;
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

	plugin_path		= CFG_DEFAULT_PLUGINS_PATH;
	keyfile			= CFG_DEFAULT_SSL_KEYFILE;
	certfile		= CFG_DEFAULT_SSL_CERTFILE;
	logfile			= NULL;

	/* parse command line arguments */
	while (1) {
		curopt_idx = 0;
		curopt = getopt_long (argc,argv,"n46Vhvb:p:l:t:c:k:x:X:P:",long_opts, &curopt_idx);
		if (curopt == -1) break;

		switch (curopt) {
			case 'v': cfg->verbose++; break;
			case 'b': cfg->iface = optarg; break;
			case 'p': cfg->port = optarg; break;
			case 'l': logfile = optarg; break;
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
			case 'x': plugin_path = optarg; break;
			case '?':
			default:
				usage (EXIT_FAILURE);
		}
		curopt_idx = 0;
	}

	/* validate command line arguments */

	/* logfile validation : if a logfile is given, use its FILE* for output */
	if (logfile) {
		cfg->logfile = realpath(logfile, NULL);
		if (cfg->logfile == NULL){
			xlog(LOG_CRITICAL, "realpath(logfile) failed: %s\n", strerror(errno));
			return -1;
		}
		if(is_readable_file(cfg->logfile)) {
			cfg->logfile_fd = fopen(cfg->logfile, "w");
			if(!cfg->logfile_fd) {
				cfg->logfile_fd = stderr;
				xlog(LOG_CRITICAL, "[-] Failed to open '%s': %s\n", cfg->logfile, strerror(errno));
				return -1;
			}
		}
	}

	/* check if nb of threads is in boundaries */
	if (cfg->nb_threads > MAX_THREADS) {
		fprintf(stderr, "Thread number invalid. Setting to default.\n");
		cfg->nb_threads = CFG_DEFAULT_NB_THREAD;
	}

	/* check plugins path */
	if (!is_valid_plugin_path(plugin_path, &cfg->plugins_path, &cfg->autoload_path))
		return -1;

#ifdef DEBUG
        xlog(LOG_DEBUG, "Valid plugin tree for '%s' and '%s'\n", cfg->plugins_path, cfg->autoload_path);
#endif

	/* check ssl certificate */
	cfg->certfile = realpath(certfile, NULL);
	if (cfg->certfile == NULL){
		xlog(LOG_CRITICAL, "realpath(certfile) failed: %s\n", strerror(errno));
		return -1;
	}
	if ( !is_readable_file(cfg->certfile) ) {
		xlog(LOG_CRITICAL, "Failed to read certificate '%s'\n", cfg->certfile);
		return -1;
	}

	/* check ssl key */
	cfg->keyfile = realpath(keyfile, NULL);
	if (cfg->certfile == NULL){
		xlog(LOG_CRITICAL, "realpath(certfile) failed: %s\n", strerror(errno));
		return -1;
	}
	if ( !is_readable_file(cfg->keyfile) ){
		xlog(LOG_CRITICAL, "Failed to read private key '%s'\n", cfg->keyfile);
		return -1;
	}

	/* check proxy values (if any) */
	if ( proxy_port && !proxy_host) {
		xlog(LOG_CRITICAL, "%s\n", "Cannot use proxy-port without proxy-host");
		return -1;
	}

	if (proxy_host) {
		cfg->proxy.host = strdup(proxy_host);
		if (!cfg->proxy.host) {
			xlog(LOG_CRITICAL, "proxy %s\n", strerror(errno));
			return -1;
		}

		cfg->proxy.port = strdup(proxy_port);
		if (!cfg->proxy.port) {
			xlog(LOG_CRITICAL, "proxy_port %s\n", strerror(errno));
			return -1;
		}
	}

	return 0;

}


/**
 * initialize config and allocate buffers
 *
 * @return -1 if an error occured
 * @return 0 on success
 */
int proxenet_init_config(int argc, char** argv)
{
	cfg = &current_config;
	proxenet_xzero(cfg, sizeof(conf_t));

	if (parse_options(argc, argv) < 0) {
		xlog(LOG_CRITICAL, "%s\n", "Failed to parse arguments");
		proxenet_free_config();
		proxenet_xzero(cfg, sizeof(conf_t));
		return -1;
	}

	return 0;
}


/**
 * deallocate all data related to cfg
 */
void proxenet_free_config()
{
	/* those calls should be safe */
	if (cfg->logfile)
		proxenet_xfree(cfg->logfile);

	if (cfg->plugins_path) {
		proxenet_xfree(cfg->plugins_path);
                proxenet_xfree(cfg->autoload_path);
        }

	if (cfg->certfile)
		proxenet_xfree(cfg->certfile);

	if (cfg->keyfile)
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
	if (retcode<0)
		return EXIT_FAILURE;


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

	/* proxenet ends here */
	proxenet_free_config();

	return (retcode == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

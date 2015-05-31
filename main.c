#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#include "core.h"
#include "utils.h"
#include "main.h"
#include "socket.h"
#include "minica.h"


/**
 * Print version and exit if required
 */
static void version(bool end)
{
        printf("%s v%s\n", PROGNAME, VERSION);
        if (end) {
                proxenet_free_config();
                exit(EXIT_SUCCESS);
        }
}


/**
 * Print usage and exit.
 */
static void usage(int retcode)
{
        FILE* fd;
        fd = (retcode == 0) ? stdout : stderr;

        /* header  */
        fprintf(fd,
                "\n"RED"SYNTAX:"NOCOLOR"\n"
                "\t%s "GREEN"[OPTIONS+]"NOCOLOR"\n"
                "\n"RED"OPTIONS:"NOCOLOR"\n",
                PROGNAME
               );

        /* general options help */
        fprintf(fd,
                BLUE"General:"NOCOLOR"\n"
                "\t-h, --help\t\t\t\tShow help\n"
                "\t-V, --version\t\t\t\tShow version\n"
                "\t-d, --daemon\t\t\t\tStart as daemon\n"
                "\t-v, --verbose\t\t\t\tIncrease verbosity (default: 0)\n"
                "\t-n, --no-color\t\t\t\tDisable colored output (better for )\n"
                "\t-l, --logfile=/path/to/logfile\t\tLog actions in file (default stdout)\n"
                "\t-x, --plugins=/path/to/plugins/dir\tSpecify plugins directory (default: '%s')\n"
                ,

                CFG_DEFAULT_PLUGINS_PATH
               );

        /* intercept options */
        fprintf(fd,
                BLUE"Intercept mode:"NOCOLOR"\n"
                "\t-I, --intercept-only\t\t\tIntercept only hostnames matching pattern (default mode)\n"
                "\t-E, --intercept-except\t\t\tIntercept everything except hostnames matching pattern\n"
                "\t-m, --pattern=PATTERN\t\t\tSpecify a hostname matching pattern (default: '%s')\n"
                "\t-N, --no-ssl-intercept\t\t\tDo not intercept any SSL traffic\n"
                "\t-i, --ie-compatibility\t\t\tUse this option only when proxy-ing IE\n"
                ,

                CFG_DEFAULT_INTERCEPT_PATTERN
               );

        /* network options */
        fprintf(fd,
                BLUE"Network:"NOCOLOR"\n"
                "\t-4 \t\t\t\t\tIPv4 only (default)\n"
                "\t-6 \t\t\t\t\tIPv6 only (default: IPv4)\n"
                "\t-t, --nb-threads=N\t\t\tNumber of threads (default: %d)\n"
                "\t-b, --bind=bindaddr\t\t\tBind local address (default: '%s')\n"
                "\t-p, --port=N\t\t\t\tBind local port file (default: '%s')\n"
                "\t-X, --proxy-host=proxyhost\t\tForward to proxy\n"
                "\t-P  --proxy-port=proxyport\t\tSpecify port for proxy (default: '%s')\n"
                "\t-D, --use-socks\t\t\t\tThe proxy to connect to is supports SOCKS4 (default: 'HTTP')\n"
                ,

                CFG_DEFAULT_NB_THREAD,
                CFG_DEFAULT_BIND_ADDR,
                CFG_DEFAULT_BIND_PORT,
                CFG_DEFAULT_PROXY_PORT
               );

        /* ssl options */
        fprintf(fd,
                BLUE"SSL:"NOCOLOR"\n"
                "\t-c, --certfile=/path/to/ssl.crt\t\tSpecify SSL cert to use (default: '%s')\n"
                "\t-k, --keyfile=/path/to/ssl.key\t\tSpecify SSL private key file to use (default: '%s')\n"
                "\t--keyfile-passphrase=MyPwd\t\tSpecify the password for this SSL key (default: '%s')\n"
                "\t--sslcli-certfile=/path/to/ssl.crt\tPath to the SSL client certificate to use\n"
                "\t--sslcli-domain=my.ssl.domain.com\tDomain to use for invoking the client certificate (default: '%s')\n"
                "\t--sslcli-keyfile=/path/to/key.crt\tPath to the SSL client certificate private key\n"
                "\t--sslcli-keyfile-passphrase=MyPwd\tSpecify the password for the SSL client certificate private key (default: '%s')\n"
                ,

                CFG_DEFAULT_SSL_CERTFILE,
                CFG_DEFAULT_SSL_KEYFILE,
                CFG_DEFAULT_SSL_KEYFILE_PWD,
                CFG_DEFAULT_SSL_CLIENT_DOMAIN,
                CFG_DEFAULT_SSL_KEYFILE_PWD
               );

        fprintf(fd, "\n");

        exit(retcode);
}


/**
 *
 */
static void help()
{
        const char* plugin_name;
        const char* plugin_ext;
        int i;

        version(false);
        printf("Written by %s\n"
               "Released under: %s\n"
               "Using library: PolarSSL %s\n"
               "Compiled by %s (%s) with support for :\n"
               ,
               AUTHOR,
               LICENSE,
               _POLARSSL_VERSION_,
               CC, SYSTEM);

        i = 0;
        while (true) {
                plugin_name = supported_plugins_str[i];
                plugin_ext  = plugins_extensions_str[i];

                if (!plugin_name || !plugin_ext)
                        break;

                printf("\t[+] 0x%.2x   "GREEN"%-15s"NOCOLOR" (%s)\n", i, plugin_name, plugin_ext);
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
 * Check that an argument is a valid file path by
 * - solving the path
 * - ensuring result is read accessible
 *
 * @param param is the parameter to validate
 * @return the real (no symlink) full path of the file it is valid
 * @return NULL in any other case
 */
static char* cfg_get_valid_file(char* param)
{
        char buf[PATH_MAX+1]={0, };

        if (!realpath(param, buf)){
                xlog(LOG_CRITICAL, "realpath(%s) failed: %s\n", param, strerror(errno));
                return NULL;
        }

        if ( !is_readable_file(buf) ){
                xlog(LOG_CRITICAL, "Failed to read private key '%s'\n", param);
                return NULL;
        }

        return proxenet_xstrdup2(buf);
}


/**
 * Checks command line arguments once and for all. Handles (de)allocation for config_t structure
 *
 * @return 0 if successfully parsed and allocated
 * @return -1
 */
static int parse_options (int argc, char** argv)
{
        int curopt, curopt_idx;
        char *logfile, *plugin_path;
        char *keyfile, *keyfile_pwd, *certfile;
        char *sslcli_keyfile, *sslcli_keyfile_pwd, *sslcli_certfile, *sslcli_domain;
        char *proxy_host, *proxy_port;
        char *intercept_pattern;
        bool use_socks_proxy;

        proxy_host = proxy_port = NULL;
        use_socks_proxy = false;

        const struct option long_opts[]  = {
                { "version",                           0, 0, 'V' },
                { "help",                              0, 0, 'h' },
                { "verbose",                           0, 0, 'v' },
                { "daemon",                            0, 0, 'd' },
                { "no-color",                          0, 0, 'n' },
                { "plugins",                           1, 0, 'x' },
                { "logfile",                           1, 0, 'l' },

                { "intercept-only",                    0, 0, 'E' },
                { "intercept-except",                  0, 0, 'I' },
                { "match",                             1, 0, 'm' },

                { "nb-threads",                        1, 0, 't' },
                { "bind",                              1, 0, 'b' },
                { "port",                              1, 0, 'p' },
                { "proxy-host",                        1, 0, 'X' },
                { "proxy-port",                        1, 0, 'P' },
                { "use-socks",                         0, 0, 'D' },

                { "certfile",                          1, 0, 'c' },
                { "keyfile",                           1, 0, 'k' },
                { "keyfile-passphrase",                1, 0, 'K' },

                { "sslcli-certfile",                   1, 0, 'z' },
                { "sslcli-domain",                     1, 0, 'Z' },
                { "sslcli-keyfile",                    1, 0, 'y' },
                { "sslcli-keyfile-passphrase",         1, 0, 'Y' },

                { "ie-compat",                         0, 0, 'i'},
                { "no-ssl-intercept",                  0, 0, 'N'},

                { 0, 0, 0, 0 }
        };

        cfg->iface		= CFG_DEFAULT_BIND_ADDR;
        cfg->port		= CFG_DEFAULT_BIND_PORT;
        cfg->logfile_fd		= CFG_DEFAULT_OUTPUT;
        cfg->nb_threads		= CFG_DEFAULT_NB_THREAD;
        cfg->use_color		= true;
        cfg->ip_version		= CFG_DEFAULT_IP_VERSION;
        cfg->try_exit		= 0;
        cfg->try_exit_max	= CFG_DEFAULT_TRY_EXIT_MAX;
        cfg->daemon		= false;

        cfg->intercept_mode	= INTERCEPT_ONLY;
        intercept_pattern	= CFG_DEFAULT_INTERCEPT_PATTERN;

        plugin_path		= CFG_DEFAULT_PLUGINS_PATH;
        logfile			= NULL;

        certfile		= CFG_DEFAULT_SSL_CERTFILE;
        keyfile			= CFG_DEFAULT_SSL_KEYFILE;
        keyfile_pwd		= CFG_DEFAULT_SSL_KEYFILE_PWD;

        cfg->certsdir		= CFG_DEFAULT_SSL_CERTSDIR;
        cfg->certskey		= CFG_DEFAULT_SSL_CERTSKEY;
        cfg->certskey_pwd	= CFG_DEFAULT_SSL_CERTSPWD;

        sslcli_certfile		= NULL;
        sslcli_domain		= CFG_DEFAULT_SSL_CLIENT_DOMAIN;
        sslcli_keyfile		= NULL;
        sslcli_keyfile_pwd	= CFG_DEFAULT_SSL_KEYFILE_PWD;

        cfg->ie_compat          = false;
        cfg->ssl_intercept      = true;


        /* parse command line arguments */
        while (1) {
                curopt_idx = 0;
                curopt = getopt_long (argc,argv,
                                      "dn46Vhvb:p:l:t:c:k:K:x:X:P:z:y:Y:IEm:NiD",
                                      long_opts, &curopt_idx);
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
                        case 'D': use_socks_proxy = true; break;
                        case 'c': certfile = optarg; break;
                        case 'k': keyfile = optarg; break;
                        case 'K': keyfile_pwd = optarg; break;
                        case 'h': help(); break;
                        case 'V': version(true); break;
                        case 'n': cfg->use_color = false; break;
                        case '4': cfg->ip_version = AF_INET; break;
                        case '6': cfg->ip_version = AF_INET6; break;
                        case 'x': plugin_path = optarg; break;
                        case 'd': cfg->daemon = true; break;
                        case 'I': cfg->intercept_mode = INTERCEPT_ONLY; break;
                        case 'E': cfg->intercept_mode = INTERCEPT_EXCEPT; break;
                        case 'm': intercept_pattern = optarg; break;
                        case 'z': sslcli_certfile = optarg; break;
                        case 'Z': sslcli_domain = optarg; break;
                        case 'y': sslcli_keyfile = optarg; break;
                        case 'Y': sslcli_keyfile_pwd = optarg; break;
                        case 'N': cfg->ssl_intercept = false; break;
                        case 'i':
                                cfg->ie_compat = true;
                                xlog(LOG_WARNING, "%s\n", "Enabling IE compatibility mode.");
                                xlog(LOG_WARNING, "%s\n", "This mode should not be used with anything but IE <10.");
                                break;

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

        /* setting the interception mode */
        cfg->intercept_pattern = proxenet_xstrdup2(intercept_pattern);
        if(!cfg->intercept_pattern)
                return -1;

#ifdef DEBUG
        xlog(LOG_DEBUG, "Interception configured as '%s' for pattern '%s'\n",
             cfg->intercept_mode==INTERCEPT_ONLY?"INTERCEPT_ONLY":"INTERCEPT_EXCEPT",
             cfg->intercept_pattern);
#endif

        /* check plugins path */
        if (!is_valid_plugin_path(plugin_path, &cfg->plugins_path, &cfg->autoload_path))
                return -1;

#ifdef DEBUG
        xlog(LOG_DEBUG, "Valid plugin tree for '%s' and '%s'\n", cfg->plugins_path, cfg->autoload_path);
#endif

        /* validate proxenet SSL configuration for interception */
        /* check ssl certificate */
        cfg->cafile = cfg_get_valid_file(certfile);
        if (!cfg->cafile)
                return -1;
        /* check ssl key */
        cfg->keyfile = cfg_get_valid_file(keyfile);
        if (!cfg->cafile)
                return -1;
        /* get the key passphrase */
        cfg->keyfile_pwd = proxenet_xstrdup2(keyfile_pwd);
        if (!cfg->keyfile_pwd)
                return -1;

        /* validate proxenet client certificate paramaters */
        /* check ssl client certificate if provided */
        if (sslcli_certfile && sslcli_keyfile) {
                cfg->sslcli_certfile = cfg_get_valid_file(sslcli_certfile);
                if (!cfg->sslcli_certfile)
                        return -1;
                /* check ssl private key associated with the client certificate */
                cfg->sslcli_keyfile = cfg_get_valid_file(sslcli_keyfile);
                if (!cfg->sslcli_keyfile)
                        return -1;
                /* get the key passphrase */
                cfg->sslcli_keyfile_pwd = proxenet_xstrdup2(sslcli_keyfile_pwd);
                if (!cfg->sslcli_keyfile_pwd)
                        return -1;
                /* get the domain */
                cfg->sslcli_domain = proxenet_xstrdup2(sslcli_domain);
                if(!cfg->sslcli_domain)
                        return -1;
        }

        /* check proxy values (if any) */
        if ( proxy_port && !proxy_host) {
                xlog(LOG_CRITICAL, "%s\n", "Cannot use proxy-port without proxy-host");
                return -1;
        }

        if (proxy_host) {
                cfg->proxy.host = proxenet_xstrdup2(proxy_host);
                if (!cfg->proxy.host) {
                        xlog(LOG_CRITICAL, "proxy %s\n", strerror(errno));
                        return -1;
                }

                cfg->proxy.port = proxenet_xstrdup2(proxy_port);
                if (!cfg->proxy.port) {
                        xlog(LOG_CRITICAL, "proxy_port %s\n", strerror(errno));
                        return -1;
                }

                if (use_socks_proxy)
                        cfg->is_socks_proxy = true;
        }

        /* become a daemon */
        if(cfg->daemon) {
                if (cfg->verbose){
                        xlog(LOG_WARNING, "%s will now run as daemon\n", PROGNAME);
                        xlog(LOG_WARNING, "%s\n", "Use `control-client.py' to interact with the process.");
                }

                if (daemon(0, 0) < 0) {
                        xlog(LOG_ERROR, "daemon failed: %s\n", strerror(errno));
                        return -1;
                }

                cfg->verbose = 0;
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
 * Free all blocks allocated during configuration parsing
 */
void proxenet_free_config()
{
        /* those calls should be safe */
        if (cfg->logfile)
                proxenet_xfree(cfg->logfile);

        proxenet_xfree(cfg->intercept_pattern);

        if (cfg->plugins_path) {
                proxenet_xfree(cfg->plugins_path);
                proxenet_xfree(cfg->autoload_path);
        }

        if (cfg->cafile)
                proxenet_xfree(cfg->cafile);

        if (cfg->keyfile){
                proxenet_xfree(cfg->keyfile);
                proxenet_xfree(cfg->keyfile_pwd);
        }

        if(cfg->logfile_fd)
                fclose(cfg->logfile_fd);

        if (cfg->proxy.host) {
                proxenet_xfree(cfg->proxy.host);
                proxenet_xfree(cfg->proxy.port);
        }

        return;
}


/**
 *
 */
int main (int argc, char **argv, char **envp)
{
        int retcode = -1;
        (void) *envp;

        /* get configuration */
        retcode = proxenet_init_config(argc, argv);
        if (retcode<0)
                return EXIT_FAILURE;

        srand(time(0));
        serial_base = rand();

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

        if (retcode == 0) {
                if (cfg->verbose)
                        xlog(LOG_INFO, "%s exits successfully\n", PROGNAME);
                return EXIT_SUCCESS;
        } else {
                if (cfg->verbose)
                        xlog(LOG_INFO, "%s exits with errors\n", PROGNAME);
                return EXIT_FAILURE;
        }

}

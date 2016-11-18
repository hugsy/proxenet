# proxenet runtime

### Let's start

Simply invoke help !

```
$ proxenet --help                                                                                                                                                                                       13:58
proxenet v0.4-master:e3d9e27
Codename: Tartiflette - 2nd fournée
Written by hugsy
Released under: GPLv2
Using library: mbedTLS 2.4.0
Compiled by Clang (Linux-4.7.0-1-amd64) with support for :
        [+] 0x00   C               (.so)
        [+] 0x01   Python2.7.12    (.py)
        [+] 0x02   Ruby2.2.0       (.rb)
        [+] 0x03   Perl5.24.1      (.pl)
        [+] 0x04   Lua5.3.1        (.lua)
        [+] 0x05   Java1.8.0_91    (.class)

SYNTAX:
        proxenet [OPTIONS+]

OPTIONS:
General:
        -h, --help                              Show help
        -V, --version                           Show version
        -d, --daemon                            Start as daemon
        -v, --verbose                           Increase verbosity (default: 0)
        -n, --no-color                          Disable colored output
        -l, --logfile=/path/to/logfile          Log actions in file (default stdout)
        -x, --plugins=/path/to/plugins/dir      Specify plugins directory (default: '~/.proxenet/plugins')
Intercept mode:
        -I, --intercept-only                    Intercept only hostnames matching pattern (default mode)
        -E, --intercept-except                  Intercept everything except hostnames matching pattern
        -m, --pattern=PATTERN                   Specify a hostname matching pattern (default: '*')
        -N, --no-ssl-intercept                  Do not intercept any SSL traffic
        -i, --ie-compatibility                  Toggle old IE compatibility mode (default: on)
Network:
        -4                                      IPv4 only (default)
        -6                                      IPv6 only (default: IPv4)
        -t, --nb-threads=N                      Number of threads (default: 20)
        -b, --bind=bindaddr                     Bind local address (default: 'localhost')
        -p, --port=N                            Bind local port file (default: '8008')
        -X, --proxy-host=proxyhost              Forward to proxy
        -P  --proxy-port=proxyport              Specify port for proxy (default: '8080')
        -D, --use-socks                         The proxy to connect to is supports SOCKS4 (default: 'HTTP')
SSL:
        -c, --certfile=/path/to/ssl.crt         Specify SSL cert to use (default: '~/.proxenet/keys/proxenet.crt')
        -k, --keyfile=/path/to/ssl.key          Specify SSL private key file to use (default: '~/.proxenet/keys/proxenet.key')
        --keyfile-passphrase=MyPwd              Specify the password for this SSL key (default: '')
        --sslcli-certfile=/path/to/ssl.crt      Path to the SSL client certificate to use
        --sslcli-domain=my.ssl.domain.com       Domain to use for invoking the client certificate (default: '*')
        --sslcli-keyfile=/path/to/key.crt       Path to the SSL client certificate private key
        --sslcli-keyfile-passphrase=MyPwd       Specify the password for the SSL client certificate private key (default: '')

```

### Explaining Runtime
When started, `proxenet` will start initializing plugins and appending them to
a list **only if** they are valid (filename convention and syntaxically
valid). Then it will start looking for events.

```
$ ./proxenet -4 -t 20 -vvv
INFO: Listening on localhost:8008
INFO: Adding Python plugin 'DeleteEncoding'
INFO: Adding Python plugin 'InjectRequest'
INFO: Adding Lua plugin 'InjectRequest'
INFO: Adding Python plugin 'Intercept'
INFO: Plugins loaded
INFO: 4 plugin(s) found
Plugins list:
|_ priority=1   id=1   type=Python    [0x0] name=DeleteEncoding       (ACTIVE)
|_ priority=2   id=2   type=Python    [0x0] name=InjectRequest        (ACTIVE)
|_ priority=2   id=3   type=Lua       [0x1] name=InjectRequest        (ACTIVE)
|_ priority=9   id=4   type=Python    [0x0] name=Intercept            (ACTIVE)
```

In this example, 4 plugins were automatically loaded and will be executed **on
every** request/response. This is important to keep in mind.

`proxenet` allows you to interact with the engine through a Unix socket, by default
located at `/tmp/proxenet-control-socket`. You can connect using `ncat`
command, and display help menu help with `help` command:
```
$ ncat -U /tmp/proxenet-control-socket
Welcome on proxenet control interface
Type `help` to list available commands
>>> help
Command list:
quit           	Leave kindly
help           	Show this menu
pause          	Toggle pause
info           	Display information about environment
verbose        	Get/Set verbose level
reload         	Reload the plugins
threads        	Show info about threads
```

For those unfortunate that do not have `ncat`, a client is provided. Simply run
the Python script `control-client.py`

Keys and associated functionalities are (I hope) explicit enough :)

Commands can also be triggered using command line (for scripting for example)
``` bash
$ echo "plugin toggle 1"| ncat -U /tmp/proxenet-control-socket
Welcome on proxenet control interface
Type `help` to list available commands
>>> Plugin 1 is now ACTIVE
$
```

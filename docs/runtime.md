# proxenet runtime

### Let's start

Simply invoke help !

```
$ ./proxenet --help
proxenet v0.2
Written by hugsy < @__hugsy__>
Released under: GPLv2

Compiled with support for :
        [+] 0x00   Python     (.py)
        [+] 0x01   C          (.so)
        [+] 0x02   Perl       (.pl)

SYNTAX :
        proxenet [OPTIONS+]

OPTIONS:
	-t, --nb-threads=N			Number of threads (default: 10)
	-b, --lbind=bindaddr			Bind local address (default: localhost)
	-p, --lport=N				Bind local port file (default: 8008)
	-l, --logfile=/path/to/logfile		Log actions in file
	-x, --plugins=/path/to/plugins/dir	Specify plugins directory (default: ./plugins)
	-X, --proxy-host=proxyhost				Forward to proxy
	-P  --proxy-port=proxyport				Specify port for proxy (default: 8080)
	-k, --key=/path/to/ssl.key		Specify SSL key to use (default: ./keys/proxenet.key)
	-c, --cert=/path/to/ssl.crt		Specify SSL cert to use (default: ./keys/proxenet.crt)
	-v, --verbose				Increase verbosity (default: 0)
	-n, --no-color				Disable colored output
	-4, 					IPv4 only (default)
	-6, 					IPv6 only (default: IPv4)
	-h, --help				Show help
	-V, --version				Show version

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

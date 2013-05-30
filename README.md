# Proxenet

The idea behind Proxenet is to have a fully DIY web proxy for pentest. It is a
C-based proxy that allows you to interact with higher level languages for
modifying on-the-fly requests/responses sent by your Web browser.

It is still at a very early stage of development (but very active though), and
supports plugins in the following languages :
- C
- Python
- Lua
- Ruby (-ish)
- Perl (-ish)

(Maybe more to come)


## Environment setup

### Compilation

Best way to set up a new `proxenet` environment is as this

```
$ git clone https://github.com/hugsy/proxenet.git
$ cd proxenet
$ make
```

Default Makefile will compile `proxenet` without any plugin support (and hence
will act as a simple relay). To enable plugins, you need to modify the Makefile
setting to `1` the language you want to support.

For example, editing `Makefile` like this
```
# PLUGINS 
WITH_C_PLUGIN		=	0
WITH_PYTHON_PLUGIN	=	1
WITH_PERL_PLUGIN	=	1
WITH_RUBY_PLUGIN	=	0
```

will enable support for Python and Perl plugins.
You will need to have the correct libraries installed on your system to compile
and link it properly (see Language Versions part).
You might also want to disable debug output. This can be done by setting to `0`
the DEBUG option in the Makefile.

Then, simply type ``` make ```


### Usage

Best way to start with `proxenet` is 
``` 
$ ./proxenet --help
proxenet v0.01
Written by hugsy < @__hugsy__>
Released under: GPLv2

Compiled with support for :
        [+] 0x00   Python     (.py)
        [+] 0x01   C          (.so)
        [+] 0x02   Perl       (.pl)

SYNTAX :
        proxenet [OPTIONS+]

OPTIONS:
        -t, --nb-threads=N                      Number of threads (default: 10)
        -b, --lbind=bindaddr                    Bind local address (default: localhost)
        -p, --lport=N                           Bind local port file (default: 8008)
        -l, --logfile=/path/to/logfile          Log actions in file
        -x, --plugins=/path/to/plugins/dir      Specify plugins directory (default: ./plugins)
        -k, --key=/path/to/ssl.key              Specify SSL key to use (default: ./keys/proxenet.key)
        -c, --cert=/path/to/ssl.crt             Specify SSL cert to use (default: ./keys/proxenet.crt)
        -v, --verbose                           Increase verbosity (default: 0)
        -n, --no-color                          Disable colored output
        -4,                                     IPv4 only (default: all)
        -6,                                     IPv6 only (default: all)
        -h, --help                              Show help
        -V, --version                           Show version
```

### Runtime

When started, `proxenet` will start initializing plugins and appending them to
a list **if** then they are valid (filename convention and syntaxically
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
INFO: Starting interactive mode, press h for help
```

In this example, 4 plugins were automatically loaded and will be executed **on
every** request/response. This is important to keep in mind.

`proxenet` allows you to interact with engine through a Unix socket, by default
located at `/tmp/proxenet-control-socket`. You can connect using `nc` or `ncat`
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

Keys and associated functionalities are explicit enough :)

Commands can also be triggered using command line (for scripting for example)
```
$ echo "plugin toggle 1"| ncat -U /tmp/proxenet-control-socket
Welcome on proxenet control interface
Type `help` to list available commands
>>> Plugin 1 is now ACTIVE
$
```

### The best of both world ?

Some people might miss the beautiful interface some other GUI-friendly proxies
provide. So be it ! Plug `proxenet` as a relay behind your favorite `Burp`,
`Zap`, `Proxystrike`, `abrupt`, etc. and enjoy the show !


## Write-Your-Own-Plugins

It is a fact that writing extension for `Burps` is a pain, and other tools only
provides plugins (when they do) in the language they were written in.
So the basic core idea behind `proxenet` is to allow pentesters to **easily**
interact with their HTTP requests/responses in their favorite high-level
language. 

### HOWTO write my plugin

It was purposely made to be extremely easy to write new plugin in your favorite
language. You just have to implement two functions respectively called (by
default) `proxenet_response_hook` and `proxenet_request_hook` which have the
following properties :
	  - take 2 arguments: an Integer (the request/response id) and a String
	  (the request/response itself)
	  - return a String

And yeah, that's it, reload `proxenet` and you are ready to go !


### New plugin example

A very simple library was created for easily modifying HTTP request and
response. It is named `pimp` (Python Interaction Module for Proxenet). We will
use it in this example to add a new header in every HTTP request.

1. Create a file in the `plugins` directory with the following syntax
```
<Priority><PluginName>.<Extension>
where
- Priority is an integer between 1-9 (1 means highest priority, 9 lowest)
- PluginName is whatever you want
- Extension is a known extension (for example "py" for Python, "rb" for Ruby,
and so on)
```
We'll create `./plugins/2InsertHeader.py`.

2. Edit `./plugins/2InsertHeader.py` to implement the required functions:

```
import pimp

def proxenet_request_hook(request_id, request):
    r = pimp.HTTPRequest(request)
    r.add_header("X-Injected-By", "python/proxenet")
    return str(r)
    
def proxenet_response_hook(request_id, request):
    return request
    
if __name__ == "__main__":
    get_request  = "GET   /   HTTP/1.1\r\nHost: foo\r\n\r\n"
    post_request = "POST / HTTP/1.1\r\nHost: foo\r\nContent-Length: 5\r\n\r\nHello"
    print proxenet_request_hook(get_request)
    print proxenet_response_hook(post_request)
```

The 'main' part is not required, but it allows you to test your plugin directly
with your interpreter.

3. And you're done ! `proxenet` will load your plugin and enable if it is
correctly syntaxed. Your new plugin will now be applied for every request ! The
interactive menu allows you to disable/re-enable it on the fly.



## Configuration

### Plugins

Some plugins are provided, mostly to demonstrate the potential of the tool. Feel
free to add your own (see Do-Your-Own-Plugins part).

### Using other SSL keys

SSL layer is fully built-in and extremely flexible thanks to the amazing library
PolarSSL (http://polarssl.org/).

- Using your own SSL keys can be done easily with command line parameters
`$ ./proxenet --key=/path/to/ssl.key --cert=/path/to/ssl.crt `

- If you don't trust me (which is a good thing), you can re-generate new SSL keys easily :
` $ make keys `

### Proxy forwarding 

`proxenet` has the possibility to be placed as intermediate between 2 proxy and
inject/modify packets transparently. 

```
$ ./proxenet -X 192.168.128.1 --proxy-port 3128
INFO: tid-139759413225216 in `xloop'(core.c:729) Starting interactive mode, press h for help
INFO: tid-139759413225216 in `xloop'(core.c:826) Infos:
- Listening interface: localhost/8008
- Supported IP version: IPv4
- Logging to stdout
- Running/Max threads: 0/10
- SSL private key: /home/hugsy/code/proxenet/keys/proxenet.key
- SSL certificate: /home/hugsy/code/proxenet/keys/proxenet.crt
- Proxy: 192.168.128.1 [3128]
- Plugins directory: /home/hugsy/code/proxenet/plugins
Plugins list:
|_ priority=1   id=1   type=Python    [0x0] name=DeleteEncoding       (ACTIVE)
|_ priority=2   id=2   type=Ruby      [0x1] name=InjectRequest        (ACTIVE)
|_ priority=2   id=3   type=Python    [0x0] name=InjectRequest        (ACTIVE)
|_ priority=9   id=4   type=Python    [0x0] name=Intercept            (ACTIVE)

```

## Languages Versions

Implemented with C API in :
- Python 2.7
- Ruby 1.8 (Ruby 1.9 and 2.0 yet to come)
- Perl 5.16
- Lua 5.2


## What's next ?

There are heaps and heaps of bugs, crashes, etc., so I am working on fixing that to
provide more stability (please tell me your bugs/patches !)
Many many many other features to come as well through plugins ! 

Want to get more ? Fork it, patch it, push it !!

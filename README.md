# Proxenet

The idea behind Proxenet is to have a fully DIY web proxy for pentest. It is a
C-based proxy that allows you to interact with higher level languages for
modifying on-the-fly requests/responses sent by your Web browser.

It is still at a very early stage of development (but very active though), and
supports plugins in the following languages :
- C
- Python
- Ruby
- Perl
- Lua

(Maybe more to come)


## Enviroment setup

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


## Usage

Best way to start with `proxenet` is 
``` 
$ ./proxenet --help
proxenet v0.01
Written by hugsy < @__hugsy__>
Released under: BeerWare

Compiled with support for :
        [+] 0x00   Python     (.py)
        [+] 0x01   C          (.so)
        [+] 0x02   Perl       (.pl)

SYNTAXE :
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

## Do-Your-Own-Plugins

Core purpose of proxenet

### HOWTO write my plugin

It was purposely made to be extremely easy to write new plugin in your favorite
language. You just have to implement two functions respectively called (by
default) `proxenet_request_hook` and `proxenet_request_hook` which have the
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
```python
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
	``` $ ./proxenet --key=/path/to/ssl.key --cert=/path/to/ssl.crt	```

	- If you don't trust me (which is a good thing), you can re-generate new
	SSL keys easily :
	``` $ make keys ```

	

## Languages Versions

Implemented with C API in :
- Python 2.7
- Ruby 1.8 (Ruby 1.9 being developed)
- Perl 5.16 (being developed)
- Lua 5.2 (being developed)


## What's next ?

Many many many other features to come ! 

# Write-Your-Own-Plugins

It is a fact that writing extension for `Burp` is a pain, and other tools only
provides plugins (when they do) in the language they were written in.
So the basic core idea behind `proxenet` is to allow pentesters to **easily**
interact with their HTTP requests/responses in their favorite high-level
language.


## HOWTO write my plugin

It was purposely made to be extremely easy to write new plugin in your favorite
language. You just have to implement two functions respectively called (by
default) `proxenet_response_hook` and `proxenet_request_hook` which have the
following properties:

- take 3 arguments
  1.   `request_id` (or resp. `response_id`) - type Integer - the request/response
  identifier. This parameter is unique for each request and allows to link a
  request to its response(s) from the server (as a response can be delivered in
  different chunks).
  2.   `request` (or resp. `response`) - type String - the
   request/response itself. The format is (depending of the interpreter) either
   a regular string or an array of bytes.
  3.   `uri` - type String - the full URI
- return a String (or array of bytes)

Simply drop the new plugin in the plugins directory either specified on the
command line (via the `-x` option), or by default in the location defined by
`CFG_DEFAULT_PLUGINS_PATH` (by default `./proxenet-plugins`).

You can then load the plugin via the client.
```bash
>>> plugin load 1MyNewPlugin.rb
Plugin '1MyNewPlugin.rb' successfully added!
```



## Step-by-Step : A basic plugin example

For the sake of simplicity, here is a very simple example of a proxenet plugin
written in Python. Note that the exact same methodology applies for plugins
written in Ruby, Lua, etc.

The demo plugin is quite dumb on purpose only to make you familiar with the way
`proxenet` proceeds with plugins.

Here, we will simply append an HTTP header in every request and response.


### Pre-requisite

Here let's assume that an instance is already up and running.

A very basic Python template for creating plugins would be like this:
```python
def proxenet_request_hook(request_id, request, uri):
    return request

def proxenet_response_hook(response_id, response, uri):
    return response

if __name__ == "__main__":
    pass
```


### The "hard" work

Create a new Python script in the plugins directory, and copy/paste this
template.

Appending an HTTP header simply means that we want to substitute the double
[CRLF](https://en.wikipedia.org/wiki/CRLF) - marking the end of the HTTP headers
blob - with our header, followed by this double CRLF.

The implementation comes then right out of the box:
```python
def proxenet_request_hook(request_id, request, uri):
    crlf = "\r\n"
    hdr  = "X-Proxenet-Rules: Python-Style"
    return request.replace(crlf * 2, crlf + hdr + crlf * 2)

def proxenet_response_hook(response_id, response, uri):
    return response

if __name__ == "__main__":
    pass
```


### Plug it in

Our plugin is ready. Simply use the client to load it (valid loaded script will
immediately become active but can be disabled through the client).

```bash
>>> plugin load 1AddHeader.py
Plugin '1AddHeader.py' successfully added!
```

Now use any site and check the headers sent. Yup, it was **that** simple.

Too easy for you ? Want a more real-life use of `proxenet`. No worries, just
keep reading


## A more advanced plugin example

Now let's do some useful stuff.

The plugin to create will log every HTTP request/response in a
[SQLite](https://sqlite.org/) database given us the possibility to
(transparently) save the whole HTTP sessions during a pentest. For the record,
this "feature" is not available in the free version of BurpSuite. So by hooking
`proxenet` behind your free Burp and using this plugin, you will never lose any
session.


### Re-use working stuff

We will use the exact same template than in the example before.
We will only rely on 2 Python modules `sqlite3` and `time`, both built-in.


### The "hard" work

We want our plugin to have a low priority, meaning that other plugins (which
potentially modify requests/responses) will be executed first.
Create a file in the plugins directory, for example `9LogReqRes.py`, and
copy/paste the template provided a few lines above.

Unfortunately, SQLite (not unlike **many** tools and libraries) does not support
multi-threading. Luckily, `proxenet` was designed to multi-thread only the
hooking functions __and nothing else__.

As a consequence, we only need to define the SQLite database object as a Python
`global` in the main thread. Like this:

```python
import time, sqlite3

class SqliteDb:
    def __init__(self, dbname="/tmp/proxenet.db"):
        self.data_file = dbname
        self.execute("CREATE TABLE requests  (id INTEGER, request BLOB, uri TEXT, timestamp INTEGER)")
        self.execute("CREATE TABLE responses (id INTEGER, response BLOB,  uri TEXT, timestamp INTEGER)")
        return

    def connect(self):
        self.conn = sqlite3.connect(self.data_file)
        self.conn.text_factory = str
        return self.conn.cursor()

    def disconnect(self):
        self.conn.close()
        return

    def execute(self, query, values=None):
        cursor = self.connect()
        cursor.execute(query, values)
        self.conn.commit()
        return cursor

db = SqliteDb()
```

By defining our database globally, the Python main thread acquires the lock for
it *but* it is still reachable by other threads.

Now we simply have to fill the functions:
```python
def proxenet_request_hook(request_id, request, uri):
    global db
    db.execute("INSERT INTO requests VALUES (?, ?, ?, ?)", (request_id, request, uri, int( time.time() )))
    return request


def proxenet_response_hook(response_id, response, uri):
    global db
    db.execute("INSERT INTO responses VALUES (?, ?, ?, ?)", (response_id, response, uri, int( time.time() )))
    return response
```

This is it! Now simply load it in `proxenet` and never loose any request/response
again! The full version of this plugin is available
[here](https://github.com/hugsy/proxenet-plugins/blob/master/9LogReqRes.py).


## Want to add your 50c ?

The GitHub repository
[proxenet-plugins](https://github.com/hugsy/proxenet-plugins) contains a few
plugins already (more will come). But if you want to share a cool plugin, feel
free to send a "Pull Request".

Thanks for using `proxenet`.

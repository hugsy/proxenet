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

And yeah, that's it, reload `proxenet` and you are ready to go!


## New plugin example

A very simple library was created for easily modifying HTTP request and
response. It is named `pimp` (Python Interaction Module for Proxenet). We will
use it in this example to add a new header in every HTTP request.


### Create the plugin

Create a file in the `plugins` directory with the following syntax
`<Priority><PluginName>.<Extension>`
where

- `Priority` is an integer between 1-9 (1 means highest priority, 9 lowest)
- `PluginName` is the name (whatever you want)
- `Extension` is a known extension (for example ***py*** for Python, ***rb*** for Ruby,
  and so on)

We'll create `./plugins/2InsertHeader.py`.


### Specify its behavior on the requests/responses

To do so, simply edit `./plugins/2InsertHeader.py` to implement the required functions:

``` python
import pimp

def proxenet_request_hook(request_id, request, uri):
    r = pimp.HTTPRequest(request)
    r.add_header("X-Injected-By", "python/proxenet")
    return str(r)

def proxenet_response_hook(response_id, response, uri):
    return response

if __name__ == "__main__":
    get_request  = "GET   /   HTTP/1.1\r\nHost: foo\r\n\r\n"
    post_request = "POST / HTTP/1.1\r\nHost: foo\r\nContent-Length: 5\r\n\r\nHello"
    print proxenet_request_hook(get_request)
    print proxenet_response_hook(post_request)
```

The `main` part is not required by `proxenet` (unlike functions
`proxenet_request_hook` and `proxenet_response_hook`) but it is a convenient way
to test your plugins directly with your interpreter before loading them into
`proxenet`. If `proxenet` detects an invalid behaviour from a plugin, it will be
automatically de-activated from runtime.


### Tadaaa
And you're done ! `proxenet` will load your plugin and enable if it is correctly
syntaxed. Your new plugin will now be applied for every request! The interactive
console allows you to disable/re-enable it on the fly.


### Want mo4r ?
Because plugins are treated sequentially, we can add some very cool
features. How about we want to have an interception plugin that will only be
spawned **after** all other plugins ? If you already written extensions in
`Burp` (or even tried to), you should know that this kind of behavior is
impossible to do. Whereas `proxenet` makes it totally painless.

   1. Create a plugin with the lowest priority (ie. 9Intercept.py)
   2. We want to have a popup coming at us with the request, so let's use PyQt4
      (a template of intercept code can be found
      [here](https://github.com/hugsy/proxenet/blob/master/examples/InterceptExample.py))
   3. ... and that's it :) Easy, right ?

#!/usr/bin/env python2.7
import os, sys, socket, argparse
import json, cgi,time, atexit

try:
    from bottle import request, route, get, post, run, static_file, redirect
except ImportError:
    sys.stderr.write("Missing package `bottle`\n")
    exit(1)

__author__    =   "hugsy"
__version__   =   0.1
__licence__   =   "GPL v.2"
__file__      =   "control-web.py"
__desc__      =   """control-web is a basic Web interface to control proxenet"""
__usage__     =   """{3} version {0}, {1}
by {2}
syntax: {3} [options] args
""".format(__version__, __licence__, __author__, __file__)

PROXENET_SOCKET_PATH = "/tmp/proxenet-control-socket"

def is_proxenet_running():
    return os.path.exists(PROXENET_SOCKET_PATH)

def not_running_html():
    return """<div class="alert alert-danger" role="alert"><b>proxenet</b> is not running</div>"""

def format_result(res):
    d = res.replace('\n', "<br>")
    d = d[:-4]
    return d

def recv_until(sock, pattern=">>> "):
    data = ""
    while True:
        data += sock.recv(1024)
        if data.endswith(pattern):
            break
    return data

def sr(msg):
    s = socket.socket( socket.AF_UNIX, socket.SOCK_STREAM )
    try:
        s.connect(PROXENET_SOCKET_PATH)
    except:
        return not_running_html()
    recv_until(s)
    s.send( msg.strip() + '\n' )
    res = recv_until(s)
    s.close()
    return format_result(res)


def build_html(**kwargs):
    header = """<!DOCTYPE html><html lang="en"><head>"""
    header+= """<script src="/js/jquery"></script>"""
    header+= """<script src="/js/bootstrap"></script>"""
    header+= """<link rel="stylesheet" href="/css/bootstrap">"""
    header+= """<link rel="stylesheet" href="/css/bootstrap-theme">"""

    header+= """<title>{}</title></head>""".format( kwargs.get("title", "") )
    body = """<body><div class="container">
    <div><img src="/img/logo"></div>
    <div class="row"><div class=col-md-12>
    <ul class="nav nav-tabs nav-justified">"""

    for path in ["info", "plugin", "threads", "config", "keys"]:
        body += """<li {2}><a href="/{0}">{1}</a></li>""".format(path, path.capitalize(),
                                                                 "class='active'" if path==kwargs.get("page") else "")

    body += """<li><a href="#" onclick="var c=confirm('Are you sure to quit?');if(c){window.location='/quit';}">Quit</a></li>"""
    body += """</ul></div></div>
    <div class="row"><div class="col-md-12"><br></div></div>
    <div class="row">
    </div><div class="col-md-12">{}</div>
    </div></body>""".format( kwargs.get("body", "") )
    footer = """</html>"""
    return header + body + footer


@route('/img/logo')
def logo(): return static_file("/proxenet-logo.png", root="./docs/img")

@route('/js/jquery')
def js_jquery(): return static_file("/jquery-1.11.2.min.js", root="./docs/html/js")
@route('/js/bootstrap')
def js_bootstrap(): return static_file("/bootstrap.min.js", root="./docs/html/js")

@route('/css/bootstrap')
def css_boostrap(): return static_file("/bootstrap.min.css", root="./docs/html/css")
@route('/css/bootstrap-theme')
def css_boostrap_theme(): return static_file("/bootstrap-theme.min.css", root="./docs/html/css")

@route('/quit')
def quit():
    if not is_proxenet_running(): return build_html(body=not_running_html())
    sr("quit")
    msg = """<div class="alert alert-warning" role="alert">Shutting down <b>proxenet</b></div><script>alert('Thanks for using proxenet');</script>"""
    return build_html(body=msg)


@route('/info')
def info():
    if not is_proxenet_running(): return build_html(body=not_running_html())
    res = sr("info")
    info = json.loads( res )['info']
    html = ""
    for section in info.keys():
        html += """<div class="panel panel-default">"""
        html += """<div class="panel-heading"><h3 class="panel-title">{}</h3></div>""".format(section)

        html += """<div class="panel-body"><ul>"""
        for k,v in info[section].iteritems():
            if type(v).__name__ == "dict":
                html += "<li>{}: <ol>".format(k)
                for a,b in v.iteritems(): html += "<li>{}: {}</li>".format(a,b)
                html += "</ol></li>"
            else:
                html += "<li>{}: {}</li>".format(k, v)
        html += "</ul></div></div>"""

    return build_html(body=html, title="Info page", page="info")


@route('/')
@route('/index')
def index():
    return info()


@get('/plugin')
def plugin():
    if not is_proxenet_running(): return build_html(body=not_running_html())
    html = ""

    # enabled plugins
    res = sr("plugin list")
    js = json.loads( res )
    html += """<div class="panel panel-default">"""
    html += """<div class="panel-heading"><h3 class="panel-title">Plugins loaded</h3></div>"""
    html += """<table class="table table-hover table-condensed">"""
    html += "<tr><th>Id</th><th>Name</th><th>Priority</th><th>Status</th><th>Action</th></tr>"
    for name in js.keys():
        p = js[name]
        html += "<tr><td>{}</td><td>{}</td><td>{}</td><td>{}</td>".format(p['id'], name, p['priority'], p['state'])
        if p['state']=="INACTIVE":
            html += """<td><button type="button" class="btn btn-default btn-xs" data-toggle="button" aria-pressed="false" onclick="window.location='/plugin/{}/enable'">Enable</button></td>""".format(p["id"])
        else:
            html += """<td><button type="button" class="btn btn-default btn-xs" data-toggle="button" aria-pressed="false" onclick="window.location='/plugin/{}/disable'">Disable</button></td>""".format(p["id"])
        html += "</tr>"
    html += "</table></div>"""

    time.sleep(0.10)

    # available plugins
    res = sr("plugin list-all")
    js = json.loads( res )
    title = js.keys()[0]
    html += """<div class="panel panel-default">"""
    html += """<div class="panel-heading"><h3 class="panel-title">{}</h3></div>""".format(title)
    html += """<table class="table table-hover table-condensed">"""
    html += "<tr><th>Name</th><th>Type</th><th>Status</th><th>Autoload?</th></tr>"
    for k,v in js[title].iteritems():
        _type, _is_loaded = v
        html += "<tr><td>{}</td><td>{}</td>".format(k, _type)
        if _is_loaded:
            html += """<td><button type="button" class="btn btn-default btn-xs" data-toggle="button" aria-pressed="true" disabled='true'>Loaded</button></td>"""
        else:
            html += """<td><button type="button" class="btn btn-default btn-xs" data-toggle="button" aria-pressed="false" onclick="window.location='/plugin/load/{}'">Load</button></td>""".format(k)

        html += "<td>"
        fpath = os.path.abspath("./proxenet-plugins/autoload/" + k)

        if os.path.islink(fpath):
            html += """<button type="button" class="btn btn-default btn-xs" data-toggle="button" aria-pressed="true" disabled='true'">Added</button>"""
            html += """<button type="button" class="btn btn-default btn-xs" data-toggle="button" aria-pressed="false" onclick="window.location='/plugin/unautoload/{}'">Remove</button>""".format(k)
        else:
            html += """<button type="button" class="btn btn-default btn-xs" data-toggle="button" aria-pressed="false" onclick="window.location='/plugin/autoload/{}'">Add</button>""".format(k)
            html += """<button type="button" class="btn btn-default btn-xs" data-toggle="button" aria-pressed="true" disabled='true'>Removed</button>"""

        html += "</td>"

        html += "</tr>"
    html += "</table></div>"""

    return build_html(body=html, title="Plugins detail", page="plugin")


@get('/plugin/load/<fname>')
def plugin_load(fname):
    if not is_proxenet_running(): return build_html(body=not_running_html())
    res = sr("plugin load {}".format(fname))
    if "error" in res:
        return build_html(body="""<div class="alert alert-danger" role="alert">Failed to load <b>{}</b></div>""".format(cgi.escape(fname)))
    else:
        return build_html(body="""<div class="alert alert-success" role="alert"><b>{}</b> loaded successfully</div>""".format(cgi.escape(fname)))

@get('/plugin/unautoload/<fname>')
def plugin_remove_from_autoload(fname):
    if not is_proxenet_running(): return build_html(body=not_running_html())
    fname = cgi.escape(fname)
    flink = os.path.realpath(".") + "/proxenet-plugins/autoload/" + fname
    if not os.path.islink(flink):
        return build_html(body="""<div class="alert alert-danger" role="alert">Failed to remove '<b>{}</b>' from autoload directory </div>""".format(fname))
    os.unlink(flink)
    return build_html(body="""<div class="alert alert-success" role="alert"><b>{}</b> successfully removed from autoload directory</div>""".format(fname))

@get('/plugin/autoload/<fname>')
def plugin_add_to_autoload(fname):
    if not is_proxenet_running(): return build_html(body=not_running_html())
    fname = cgi.escape(fname)
    fpath = os.path.abspath("./proxenet-plugins/autoload/" + fname)
    if not os.path.isfile(fpath):
        return build_html(body="""<div class="alert alert-danger" role="alert">Failed to load <b>{}</b></div>""".format(fname))
    flink = os.path.realpath("./proxenet-plugins/autoload/" + fname)
    os.symlink(fpath, flink)
    return build_html(body="""<div class="alert alert-success" role="alert"><b>{}</b> loaded successfully</div>""".format(fname))

@get('/plugin/<id:int>/<action>')
def plugin_toggle(id,action):
    if not is_proxenet_running(): return build_html(body=not_running_html())
    res = sr("plugin set {} toggle".format(id))
    if "error" in res:
        return build_html(body="""<div class="alert alert-danger" role="alert">Failed to change state for plugin {}</div>""".format(id))
    else:
        return build_html(body="""<div class="alert alert-success" role="alert">Plugin {} state changed</div>""".format(id))


@get('/threads')
def threads():
    if not is_proxenet_running(): return build_html(body=not_running_html())
    res = sr("threads")
    js = json.loads( res )
    title = js.keys()[0]
    html = """<div class="panel panel-default">"""
    html += """<div class="panel-heading"><h3 class="panel-title">{}</h3></div>""".format(title)
    html += """<div class="panel-body"><ul>"""
    for k,v in js[title].iteritems():
        if v.__class__.__name__ == "dict":
            html += "<li>{}:<ol>".format(k)
            for a,b in v.iteritems(): html += "<li>{} &#8594; {}</li>".format(a,b)
            html += "</li>"
        else:
            html += "<li>{}: {}</li>".format(k, v)
    html += "</ul></div></div>"""

    html += """<div class="col-lg-6"><div class="btn-group" role="group" aria-label="">
    <button type="button" class="btn btn-default" onclick="window.location='/threads/inc'">Increment</button>
    <button type="button" class="btn btn-default" onclick="window.location='/threads/dec'">Decrement</button>
    </div></div><div class="col-lg-6">
    <div class="input-group">
    <form method="GET" action="#">
    <input type="text" class="form-control" placeholder="Number of threads">
    <span class="input-group-btn">
    <button class="btn btn-default" type="button">Apply</button>
    </span></div></form></div>"""
    return build_html(body=html, title="Get information on Threads", page="threads")


@get('/threads/<word>')
def threads_set(word):
    if not is_proxenet_running(): return build_html(body=not_running_html())
    if word not in ("inc", "dec", "set"):
        return "Error"
    res = sr("threads {}".format(word))
    redirect("/threads")
    return


@route('/keys')
def keys():
    extensions = [ ("PEM formatted certificate", "crt"), ("PEM formatted private key", "key"),
                   ("PKCS12 formatted certificate+key", "p12"), ("PKCS7 formatted certificate", "p7b"), ]
    html = """<div class="panel panel-default">"""
    html+= """<div class="panel-heading"><h3 class="panel-title">SSL keys</h3></div>"""
    html+= """<div class="panel-body"><ul>"""
    html+= """"""
    for ext,fmt in extensions:
        html += "<li><a href=\"/keys/proxenet.{}\">{}</a></li>".format(fmt, ext)
    html+= "</ul></div>"
    return build_html(body=html, title="Get proxenet SSL keys", page="keys")

@route('/keys/proxenet.<fmt>')
def key(fmt):
    if fmt in ("crt", "key", "p12", "p7b"):
        return static_file("/proxenet.%s" % fmt, root="./keys")

@route('/config')
def config():
    if not is_proxenet_running(): return build_html(body=not_running_html())
    res = sr("config list")
    js = json.loads( res )
    title = js.keys()[0]
    values = js[title]

    # view config
    html  = """<div class="panel panel-default">"""
    html += """<div class="panel-heading"><h3 class="panel-title">{}</h3></div>""".format( title )
    html += """<div class="panel-body">"""
    html += """<table class="table table-hover table-condensed">"""
    html += """<tr><th>Setting</th><th>Value</th><th>Type</th></tr>"""
    for k,v in values.iteritems():
        _val, _type = v["value"], v["type"]
        html += "<tr><td><code>{}</code></td><td>{}</td><td>{}</td></tr>".format(k,_val,_type)
    html += "</table></div></div>"""

    # edit config
    html += """<div class="panel panel-default">"""
    html += """<div class="panel-heading"><h3 class="panel-title">Change Value</h3></div>"""
    html += """<div class="panel-body"><form class="form-inline" action="/config/set" method="get">"""
    html += """<div class="form-group"><select class="form-control" name="setting">"""
    for k,v in values.iteritems():
        html += """<option>{}</option>""".format( k )
    html += """</select></div><div class="form-group"><label class="sr-only">Value</label><input class="form-control" name="value" placeholder="Value"></div><button type="submit" class="btn btn-default">Change Value</button>"""
    html += """</form></div></div>"""

    return build_html(body=html, title="proxenet Configuration", page="config")

@route('/config/set')
def config_set():
    if not is_proxenet_running(): return build_html(body=not_running_html())
    param = request.params.get("setting")
    value = request.params.get("value")
    print param, value
    res = sr("config set {} {}".format(param, value))
    js = json.loads( res )
    retcode = js.keys()[0]
    if retcode == "error":
        return """<div class="alert alert-danger" role="alert">{}</div>""".format(res[retcode])
    redirect("/config")
    return

def kill_daemon():
    os.remove( os.getpid() )
    return

def daemon():
    pid = os.fork()
    if pid > 0:
        sys.exit(0)

    os.setsid()
    os.umask(0)

    pid = os.fork()
    if pid > 0:
        sys.exit(0)

    sys.stdout.flush()
    sys.stderr.flush()
    si = file("/dev/null", 'r')
    so = file("/dev/null", 'a+')
    se = file("/dev/null", 'a+', 0)
    os.dup2(si.fileno(), sys.stdin.fileno())
    os.dup2(so.fileno(), sys.stdout.fileno())
    os.dup2(se.fileno(), sys.stderr.fileno())

    atexit.register(kill_daemon)
    return

if __name__ == "__main__":
    parser = argparse.ArgumentParser(usage = __usage__, description = __desc__)

    parser.add_argument("-v", "--verbose", default=False, action="store_true",
                        dest="verbose", help="Increments verbosity")

    parser.add_argument("-d", "--debug", default=False, action="store_true",
                        dest="debug", help="Enable `bottle` debug mode")

    parser.add_argument("-D", "--demonize", default=False, action="store_true",
                        dest="daemon", help="Daemonize the web server")

    parser.add_argument("-H", "--host", default="localhost", type=str, dest="host",
                        help="IP address to bind")

    parser.add_argument("-P", "--port", default=8009, type=int, dest="port",
                        help="port to bind")

    args = parser.parse_args()

    if args.daemon:
        print("Starting web server as daemon on %s:%d" % (args.host,args.port))
        daemon()

    run(host=args.host, port=args.port, debug=args.debug)

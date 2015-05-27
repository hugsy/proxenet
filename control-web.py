#!/usr/bin/env python2.7
import os, sys, socket, argparse
import json

try:
    from bottle import request, route, get, post, run, static_file
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
    header = """<!DOCTYPE html><html lang="en"><head><link rel="stylesheet" href="//maxcdn.bootstrapcdn.com/bootstrap/3.3.4/css/bootstrap.min.css"><link rel="stylesheet" href="//maxcdn.bootstrapcdn.com/bootstrap/3.3.4/css/bootstrap-theme.min.css"><script src="//maxcdn.bootstrapcdn.com/bootstrap/3.3.4/js/bootstrap.min.js"></script><script src="//jquery.com/jquery-wp-content/themes/jquery/js/jquery-1.11.2.min.js"></script>
    <title>{}</title></head>""".format( kwargs.get("title", "") )
    body = """<body><div class="container">
    <div><img src="/img/logo"></div>
    <div class="row"><div class=col-md-12>
    <ul class="nav nav-tabs nav-justified">"""

    for path in ["info", "plugin", "threads", "config", ]:
        body += """<li {2}><a href="/{0}">{1}</a></li>""".format(path,
                                                                 path.capitalize(),
                                                                 "class='active'" if path==kwargs.get("page") else "")

    body += """<li><a href="#" onclick="var c=confirm('Are you sure to quit?');if(c){window.location='/quit';}">Quit</a></li>"""
    body += """</ul></div></div>
    <div class="row"><div class="col-md-12"><br></div></div>
    <div class="row">
    <div class="col-md-2"></div><div class="col-md-8">{}</div><div class="col-md-2"></div>
    </div></body>""".format( kwargs.get("body", "") )
    footer = """</html>"""
    return header + body + footer


@route('/img/logo')
def logo():
    return static_file("/proxenet-logo.png", root="./docs/img")


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
    res = sr("plugin list-all")
    js = json.loads( res )
    title = js.keys()[0]
    html = """<div class="panel panel-default">"""
    html += """<div class="panel-heading"><h3 class="panel-title">{}</h3></div>""".format(title)
    html += """<table class="table table-hover table-condensed">"""
    html += "<tr><th>Name</th><th>Type</th></tr>"
    for k,v in js[title].iteritems():
        _type, _is_loaded = v
        html += "<tr><td>{}</td><td>{}{}</td></tr>".format(k, _type, " <b>Loaded</b>" if _is_loaded else "")
    html += "</table></div>"""

    return build_html(body=html, title="List all plugins available", page="plugin")


@get('/threads')
def threads():
    if not is_proxenet_running(): return build_html(body=not_running_html())
    res = sr("threads")
    js = json.loads( res )
    title = js.keys()[0]
    html = """<div class="panel panel-default">"""
    html += """<div class="panel-heading"><h3 class="panel-title">{}</h3></div>""".format(title)
    html += """<div class="panel-body"><ul>"""
    for k,v in js[title].iteritems(): html += "<li>{}: {}</li>".format(k, v)
    html += "</ul></div></div>"""

    html += """<div class="col-lg-6"><div class="btn-group" role="group" aria-label="">
    <button type="button" class="btn btn-default">Increment</button>
    <button type="button" class="btn btn-default">Decrement</button>
    </div></div><div class="col-lg-6">
    <div class="input-group">
      <input type="text" class="form-control" placeholder="Number of threads">
      <span class="input-group-btn">
        <button class="btn btn-default" type="button">Apply</button>
      </span></div></div>"""

    return build_html(body=html, title="Get information on Threads", page="threads")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(usage = __usage__, description = __desc__)

    parser.add_argument("-v", "--verbose", default=False, action="store_true",
                        dest="verbose", help="Increments verbosity")

    parser.add_argument("-d", "--debug", default=False, action="store_true",
                        dest="debug", help="Enable `bottle` debug mode")

    parser.add_argument("-H", "--host", default="localhost", type=str, dest="host",
                        help="IP address to bind")

    parser.add_argument("-P", "--port", default=8009, type=int, dest="port",
                        help="port to bind")

    args = parser.parse_args()

    run(host=args.host, port=args.port, debug=args.debug)

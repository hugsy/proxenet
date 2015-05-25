#!/usr/bin/env python2.7
import os, sys, socket, argparse

try:
    from bottle import request, route, template, run, static_file
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
    return res

def format_result(res):
    d = res.replace('\n', "<br>")
    d = d[:-4]
    return d

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
    <div class="row">
    <div class="col-md-2"></div>
    <div class="col-md-8">{}</div>
    <div class="col-md-2"></div>
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
    fmt = format_result(res)
    return build_html(body=fmt, title="Info page", page="info")


@route('/')
@route('/index')
def index():
    return info()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(usage = __usage__,
                                     description = __desc__)

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

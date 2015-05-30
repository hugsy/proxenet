#!/usr/bin/env python2
# -*- coding: utf-8 -*-
# -*- mode: python -*-

import argparse, socket, datetime, os, json, rlcompleter, readline, pprint


__author__    =   "hugsy"
__version__   =   0.1
__licence__   =   "WTFPL v.2"
__file__      =   "control-client.py"
__desc__      =   """control-client.py"""
__usage__     =   """%prog version {0}, {1}
by {2}
syntax: {3} [options] args
""".format(__version__, __licence__, __author__, __file__)

# the socket path can be modified in config.h.in
PROXENET_SOCKET_PATH = "/tmp/proxenet-control-socket"

def now():
    return datetime.datetime.now().strftime("%Y/%m/%d %H:%M:%S")

def _log(p,s):
    print "[%s] %s: %s" % (p, now(), s)
    return

def ok(msg):
    return _log('+', msg)

def info(msg):
    return _log('*', msg)

def warn(msg):
    return _log('!', msg)

def err(msg):
    return _log('-', msg)


def recv_until(sock, pattern=">>> "):
    data = ""
    while True:
        data += sock.recv(1024)
        if data.endswith(pattern):
            break
    return data


if __name__ == "__main__":
    parser = argparse.ArgumentParser(usage = __usage__,
                                     description = __desc__)

    parser.add_argument("-v", "--verbose", default=False,
                        action="store_true", dest="verbose",
                        help="increments verbosity")

    args = parser.parse_args()

    if not os.path.exists(PROXENET_SOCKET_PATH):
        err("Socket does not exist.")
        warn("Is proxenet started?")
        exit(1)

    try:
        cli = socket.socket( socket.AF_UNIX, socket.SOCK_STREAM )
        cli.settimeout(5)
        cli.connect(PROXENET_SOCKET_PATH)
    except socket.error as se:
        err("Failed to connect: %s" % se)
        exit(1)

    info("Connected")
    do_loop = True

    try:
        while True:
            if not do_loop:
                recv_until(cli)
                break

            res = recv_until(cli)
            data, prompt = res[:-4], res[-4:]
            try:
                js = json.loads( data )
                for k,v in js.iteritems():
                    if v.__class__.__name__ == "dict":
                        print("{}".format(k))
                        for a,b in v.iteritems(): print("\t{} -> {}".format(a,b))
                    else:
                        print("{} -> {}".format(k,v))
            except:
                print(data)
            cmd = raw_input( prompt )
            cli.send(cmd.strip()+"\n")
            if cmd.strip() == "quit":
                do_loop = False

    except KeyboardInterrupt:
        info("Exiting client")
    except EOFError:
        info("End of stream")
    except Exception as e:
        err("Unexpected exception: %s" % e)
    finally:
        cli.close()

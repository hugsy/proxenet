#!/usr/bin/env python2
# -*- coding: utf-8 -*-
# -*- mode: python -*-

import argparse
import socket
import datetime
import os

__author__    =   "hugsy"
__version__   =   0.1
__licence__   =   "WTFPL v.2"
__file__      =   "control-client.py"
__desc__      =   """control-client.py"""
__usage__     =   """%prog version {0}, {1}
by {2}
syntax: {3} [options] args
""".format(__version__, __licence__, __author__, __file__)

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
        cli.connect(PROXENET_SOCKET_PATH)
    except socket.error as se:
        err("Failed to connect: %s" % se)
        exit(1)

    info("Connected")

    try:
        data = cli.recv(1024).strip()
        while True:
            cmd = raw_input(data)
            cli.send(cmd)
            data = cli.recv(1024).strip()
            print(data)
            data = cli.recv(10)

    except KeyboardInterrupt:
        info("Exiting client")
    except EOFError:
        info("End of stream")
    finally:
        cli.close()

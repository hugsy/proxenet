#!/usr/bin/env python2
# -*- coding: utf-8 -*-
# -*- mode: python -*-

import argparse, socket, datetime, os, json, rlcompleter, readline, pprint
import logging, sys


__author__    =   "hugsy"
__version__   =   0.1
__licence__   =   "WTFPL v.2"
__file__      =   "control-client.py"
__desc__      =   """control-client.py"""
__usage__     =   """%prog version {0}, {1}
by {2}
syntax: {3} [options] args
""".format(__version__, __licence__, __author__, __file__)

FORMAT = '%(asctime)-15s - %(levelname)s - %(message)s'
logging.basicConfig(level=logging.INFO,format=FORMAT)

# the socket path can be modified in config.h.in
PROXENET_SOCKET_PATH = "/tmp/proxenet-control-socket"


class ProxenetClientCompleter(object):

    def __init__(self, options):
        self.options = sorted(options)
        return

    def complete(self, text, state):
        response = None
        if state == 0:
            if text:
                self.matches = [s for s in self.options if s and s.startswith(text)]
                logging.debug('{:s} matches: {:s}'.format(repr(text), self.matches))
            else:
                self.matches = self.options[:]
                logging.debug('(empty input) matches: {:s}'.format(self.matches))

        try:
            response = self.matches[state]
        except IndexError:
            response = None

        return response


def connect(sock_path):
    if not os.path.exists(sock_path):
        logging.critical("Socket does not exist.")
        logging.error("Is proxenet started?")
        return None

    try:
        sock = socket.socket( socket.AF_UNIX, socket.SOCK_STREAM )
        sock.settimeout(5)
        sock.connect(sock_path)
    except socket.error as se:
        logging.error("Failed to connect: %s" % se)
        return None

    logging.info("Connected")
    return sock


def recv_until(sock, pattern=">>> "):
    data = ""
    while True:
        data += sock.recv(1024)
        if data.endswith(pattern):
            break
    return data


def list_commands(sock):
    banner = recv_until(sock)
    sock.send("help\n")
    res = recv_until(sock)
    data, prompt = res[:-4], res[-4:]
    js = json.loads( data )
    cmds = js["Command list"].keys()
    sock.send("\n")
    return cmds


def input_loop(cli):
    if not cli:
        return

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
                print( json.dumps(js, sort_keys=True, indent=4, separators=(',', ': ')) )
            except:
                print(data)
            cmd = raw_input( prompt )
            cli.send(cmd.strip()+"\n")
            if cmd.strip() == "quit":
                do_loop = False

    except KeyboardInterrupt:
        logging.info("Exiting client")
    except EOFError:
        logging.info("End of stream")
    except Exception as e:
        logging.error("Unexpected exception: %s" % e)
    finally:
        cli.close()

    return


if __name__ == "__main__":
    parser = argparse.ArgumentParser(usage = __usage__,
                                     description = __desc__)

    parser.add_argument("-v", "--verbose", default=False,
                        action="store_true", dest="verbose",
                        help="increments verbosity")

    parser.add_argument("-s", "--socket-path", default=PROXENET_SOCKET_PATH, dest="sock_path",
                        help="path to proxenet control Unix socket")

    args = parser.parse_args()

    sock = connect(args.sock_path)
    if sock is None:
        sys.exit(1)

    command_list = list_commands(sock)
    logging.info("Loaded %d commands" % len(command_list))
    readline.parse_and_bind("tab: complete")
    readline.set_completer(ProxenetClientCompleter(command_list).complete)
    input_loop(sock)
    sys.exit(0)

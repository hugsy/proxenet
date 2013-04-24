# Proxenet Interaction Module for Python (PIMP)
#
# Small set of functions for parsing easily http request
#

import re

CRLF = "\r\n"


class HTTPRequest :

    def __init__(self, r):
        self.__method = "GET"
        self.__path = "/"
        self.__protocol = "HTTP/1.1"
        self.__host = ""
        self.__port = 80
        self.__schema = "http"
        self.__headers = {}
        self.__post_data = ""

        self.parse(r)

        
    def parse(self, req):
        i = req.find(CRLF*2)
        self.__post_data = req[i+len(CRLF*2):]
        
        headers = req[:i].split(CRLF)
        parts = re.findall(r"^(?P<method>.+?)\s+(?P<path>.+?)\s+(?P<protocol>.+?)$", headers[0])[0]
        self.__meth, self.__path, self.__protocol = parts

        for header in headers[1:]:
            key, value = re.findall(r"^(?P<key>.+?)\s*:\s*(?P<value>.+?)\s*$", header)[0]
            self.add_header(key, value)

            
    def add_header(self, key, value):
        self.__headers[key] = value

        
    def __str__(self):
        head = "{0} {1} {2}".format(self.__meth, self.__path, self.__protocol)
        hrds = [ "{0}: {1}".format(k,v) for k,v in self.__headers.iteritems()]
        data = self.__post_data

        if len(data):
            request = CRLF.join([head, ] + hrds + ['', data])
        else:
            request = CRLF.join([head, ] + hrds + [CRLF, ])

        return request
        
        

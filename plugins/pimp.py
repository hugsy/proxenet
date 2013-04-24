# Proxenet Interaction Module for Python (PIMP)
#
# Small set of functions for parsing easily http request
#

import re

CRLF = "\r\n"


class HTTPRequest :

    def __init__(self, r):
        self.__method 	= "GET"
        self.__path 	= "/"
        self.__protocol = "HTTP/1.1"
        self.__headers	= {}
        self.__body	= ""

        self.parse(r)

        
    def parse(self, req):
        i = req.find(CRLF*2)
        self.__body = req[i+len(CRLF*2):]
        
        headers = req[:i].split(CRLF)
        parts = re.findall(r"^(?P<method>.+?)\s+(?P<path>.+?)\s+(?P<protocol>.+?)$", headers[0])[0]
        self.__meth, self.__path, self.__protocol = parts

        for header in headers[1:]:
            key, value = re.findall(r"^(?P<key>.+?)\s*:\s*(?P<value>.+?)\s*$", header)[0]
            self.add_header(key, value)


    def has_header(self, key):
        return key in self.__headers.keys()
        

    def add_header(self, key, value):
        self.__headers[key] = value

        
    def del_header(self, key):
        del self.__headers[key]

        
    @property
    def headers(self):
        return self.__headers

        
    def __str__(self):
        head = "{0} {1} {2}".format(self.__meth, self.__path, self.__protocol)
        hrds = [ "{0}: {1}".format(k,v) for k,v in self.__headers.iteritems()]
        data = self.__body

        if len(data):
            request = CRLF.join([head, ] + hrds + ['', data])
        else:
            request = CRLF.join([head, ] + hrds + [CRLF, ])

        return request
        
        

class HTTPResponse :

    def __init__(self, r):
        self.__protocol	= "HTTP/1.1"
        self.__status 	= 200
        self.__reason	= "Ok"
        self.__headers 	= {}
        self.__body 	= ""

        self.parse(r)

        
    def parse(self, res):
        i = res.find(CRLF*2)
        self.__body = res[i+len(CRLF*2):]
        
        headers = res[:i].split(CRLF)
        parts = re.findall(r"^(?P<protocol>.+?)\s+(?P<status>.+?)\s+(?P<reason>.*?)$", headers[0])[0]
        self.__protocol, self.__status, self.__reason = parts

        for header in headers[1:]:
            key, value = re.findall(r"^(?P<key>.+?)\s*:\s*(?P<value>.+?)\s*$", header)[0]
            self.add_header(key, value)


    def has_header(self, key):
        return key in self.__headers.keys()


    def add_header(self, key, value):
        self.__headers[key] = value

        
    def del_header(self, key):
        del self.__headers[key]

    @property
    def headers(self):
        return self.__headers

    def __str__(self):
        head = "{0} {1} {2}".format(self.__protocol, self.__status, self.__reason)
        hrds = [ "{0}: {1}".format(k,v) for k,v in self.__headers.iteritems()]
        data = self.__body

        if len(data):
            request = CRLF.join([head, ] + hrds + ['', data])
        else:
            request = CRLF.join([head, ] + hrds + [CRLF, ])

        return request

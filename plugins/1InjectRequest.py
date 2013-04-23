def pre_request_hook(req):
    r = req.replace("\r\n\r\n", "\r\nX-Intercepted-By: proxenet\r\n\r\n")
    print r
    return r

    
def post_request_hook(req):
    return req
    

if __name__ == "__main__":
    pre_request_hook("GET / HTTP/1.1\r\nHost: foo\r\n\r\n")

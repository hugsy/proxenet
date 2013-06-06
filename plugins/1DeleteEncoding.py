import pimp

def proxenet_request_hook(request_id, request):
    r = pimp.HTTPRequest(request)
    if r.has_header("Accept-Encoding"):
        r.del_header("Accept-Encoding")
        
    return str(r)

    
def proxenet_response_hook(response_id, response):
    return response
    

if __name__ == "__main__":
    req = "GET   /   HTTP/1.1\r\nHost: foo\r\nAccept-Encoding: gzip, deflate\r\n\r\n"
    res = "HTTP/1.1 404 Not Found\r\nServer: foo\r\n\r\n"
    
    print ("%s", proxenet_request_hook(1, req))
    print ("%s", proxenet_reponse_hook(1, res))
    

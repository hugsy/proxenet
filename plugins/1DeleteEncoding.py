import pimp

def proxenet_request_hook(request_id, request):
    r = pimp.HTTPRequest(request)
    if r.has_header("Accept-Encoding"):
        r.del_header("Accept-Encoding")
        
    return str(r)

    
def proxenet_response_hook(response_id, response):
    r = pimp.HTTPResponse(response)
    r.add_header("Server", "pr0x3n7")
    return str(r)
    

if __name__ == "__main__":
    req = "GET   /   HTTP/1.1\r\nHost: foo\r\nAccept-Encoding: gzip, deflate\r\n\r\n"
    res = "HTTP/1.1 404 Not Found\r\nServer: foo\r\n\r\n"
    
    print proxenet_request_hook(1, req)
    print proxenet_reponse_hook(1, res)
    

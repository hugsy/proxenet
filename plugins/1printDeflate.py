import zlib 
from ti_utils import hexdump as xd
import subprocess

def pre_request_hook(http_request):
    i = http_request.find("\r\n\r\n")
    if i == -1:
        # should never happen
        return http_request
        
    post_data = http_request[i+4:]
    if len(post_data) == 0:
        # end of request = no post data
        return http_request

    try :
        data = zlib.decompress(post_data)
    except zlib.error:
        data = post_data[:-2]

    subprocess.call ( "/usr/bin/xmessage '%s'" % xd(data))
    
    return http_request


if __name__ == "__main__":

    # invalid 
    pre_request_hook("""GET / HTTP/1.1\r
Host: test\r""")
    
    # valid but no data
    pre_request_hook("""GET / HTTP/1.1\r
Host: test\r
\r\n""")

    # valid with data in plain text
    pre_request_hook("""GET / HTTP/1.1\r
Host: test\r
\r
plopplop\r\n""")

    # valid with data in deflate data
    pre_request_hook("""GET / HTTP/1.1\r
Host: test\r
\r
%s\r\n""" % zlib.compress("Hello World"))
    
    

def proxenet_request_hook(request_id, request)
  @CRLF = "\r\n"
  request.sub(@CRLF*2, @CRLF+"X-Injected-By: ruby/proxenet"+@CRLF*2)
end

def proxenet_response_hook(response_id, response)
  puts "From Ruby, in response #{response_id}"
  response 
end


if __FILE__ == $0
  puts "In main"
  @rid = 11
  @req  = "GET / HTTP/1.1\r\nHost: foo\r\n\r\n"
  puts proxenet_request_hook(@rid, @req)
end

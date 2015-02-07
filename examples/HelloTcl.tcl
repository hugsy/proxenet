#! /bin/env tclsh
#
# Example TCL script for proxenet
#


proc proxenet_request_hook {request_id request uri} {
    regsub -all "\r\n\r\n" $request "\r\nX-Tcl-Injected: proxenet\r\n\r\n" request
    return $request
}

proc proxenet_response_hook {response_id response uri} {
    return $response
}

set rid 42
set request "GET / HTTP/1.1\r\nHost: foo\r\n\r\n"
set uri "http://foo"

puts "Request before"
puts $request
puts "----------------------------------------"
puts "Request after"
set ret [proxenet_request_hook $rid $request $uri]
puts $ret

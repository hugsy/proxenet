sub proxenet_request_hook {
    my ($rid, $req) = @_;
    $crlf = "\r\n";
    $end = $crlf . $crlf;
    
    $req =~ s/$end/${crlf}X-Injected-By: perl\/proxenet${end}/;
    return $req
}

sub proxenet_response_hook {
    my ($rid, $res) = @_;
    return $res;
}


$rid = 14;
$req = "GET / HTTP/1.1\r\nHost: foo\r\n\r\n";

print proxenet_request_hook($rid, $req);

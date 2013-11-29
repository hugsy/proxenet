package InjectRequest;
# First line must be a package name unique across perl plugins
# See last line


sub proxenet_request_hook {
    my ($rid, $req, $uri) = @_;
    $crlf = "\r\n";
    $end = $crlf . $crlf;
    
    $req =~ s/${end}/${crlf}X-Perl-Injected: proxenet${end}/;
    return $req
}

sub proxenet_response_hook {
    my ($rid, $res, $uri) = @_;
    return $res;
}



# Unless called within proxenet
unless(caller) {
    # In order to test your script
    $rid = 14;
    $req = "GET / HTTP/1.1\r\nHost: foo\r\n\r\n";

    print proxenet_request_hook($rid, $req);
}


# Last line must be the package name between (double-)quotes
"InjectRequest";

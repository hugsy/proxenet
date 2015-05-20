# Perl plugin

This page will explain how to write a Perl plugin for `proxenet`.


## Plugin skeleton

```perl
package MyPlugin;

our $AUTHOR = "";
our $PLUGIN_NAME = "";

sub proxenet_request_hook {
    my ($request_id, $request, $uri) = @_;
    return $request;
}

sub proxenet_response_hook {
    my ($response_id, $response, $uri) = @_;
    return $response;
}

unless(caller) {
    # use for test cases
}

"MyPlugin";
```


## Example

```perl
package AddHeader;

our $AUTHOR = "hugsy";
our $PLUGIN_NAME = "AddHeader";

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

unless(caller) {
    # use for test cases
}

"AddHeader";
```

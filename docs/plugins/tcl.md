# Tcl plugin

This page will explain how to write a Tcl plugin for `proxenet`.


## Plugin skeleton

```tcl
AUTHOR = ""
PLUGIN_NAME = ""

proc proxenet_request_hook {request_id request uri} {
    return $request
}

proc proxenet_response_hook {response_id response uri} {
    return $response
}

# add test cases here
```


## Example

```tcl
AUTHOR = "hugsy"
PLUGIN_NAME = "AddHeader"

proc proxenet_request_hook {request_id request uri} {
    regsub -all "\r\n\r\n" $request "\r\nX-Powered-By: tcl-proxenet\r\n\r\n" request
    return $request
}

proc proxenet_response_hook {response_id response uri} {
    return $response
}

# add test cases here
```

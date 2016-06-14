# Ruby plugin

This page will explain how to write a Ruby plugin for `proxenet`.


## Plugin skeleton

```ruby

module MyPlugin

    $AUTHOR = ""
    $PLUGIN_NAME = ""

    module_function

    def proxenet_on_load()
        return
    end

    def proxenet_on_leave()
        return
    end

    def proxenet_request_hook(request_id, request, uri)
        return request
    end

    def proxenet_response_hook(response_id, response, uri)
        return response
    end

end

if __FILE__ == $0
    # use for test cases
end
```


## Example

```ruby

module AddHeader

    $AUTHOR = "hugsy"
    $PLUGIN_NAME = "AddHeader"

    module_function

    def proxenet_on_load()
        return
    end

    def proxenet_on_leave()
        return
    end

    def proxenet_request_hook(request_id, request, uri)
        @CRLF = "\r\n"
        @header = "X-Powered-By: ruby-proxenet"
        return request.sub(@CRLF*2, @CRLF+header+@CRLF*2)
    end

    def proxenet_response_hook(response_id, response, uri)
        return response
    end

end

if __FILE__ == $0
    # use for test cases
end
```

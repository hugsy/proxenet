# Lua plugin

This page will explain how to write a Lua plugin for `proxenet`.


## Plugin skeleton

```lua
AUTHOR = ""
PLUGIN_NAME = ""

function proxenet_request_hook (request_id, request, uri)
    local CRLF = "\r\n"
    return string.gsub(request,
			CRLF..CRLF,
			CRLF .. "X-Lua-Injected: proxenet" .. CRLF..CRLF)
end

function proxenet_response_hook (response_id, response, uri)
	return response
end

```


## Example

```lua
AUTHOR = "hugsy"
PLUGIN_NAME = "AddHeader"

function proxenet_request_hook (request_id, request, uri)
    local CRLF = "\r\n"
    local header = "X-Powered-By: lua-proxenet"
    return string.gsub(request,
			CRLF .. CRLF,
			CRLF .. header .. CRLF..CRLF)
end

function proxenet_response_hook (response_id, response, uri)
	return response
end
```

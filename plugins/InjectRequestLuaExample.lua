-- test functions with
-- $ lua -e 'RunTests = true;' script.lua
--

function proxenet_request_hook (request_id, request)
    local CRLF = "\r\n"
    return string.gsub(request,
			CRLF..CRLF,
			CRLF .. "X-Lua-Injected: proxenet" .. CRLF..CRLF)
end

function proxenet_response_hook (response_id, response)
	return response
end

--if not pcall(getfenv, 4) then
--   local rid = 12
--   local req = "GET / HTTP/1.1\r\nHost: foo\r\n\r\n"

--   local ret = proxenet_request_hook(rid, req)
--   print (ret)
--end

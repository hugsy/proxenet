# Python plugin

This page will explain how to write a Python plugin for `proxenet`.


## Plugin skeleton

```python

AUTHOR = ""
PLUGIN_NAME = ""


def proxenet_request_hook(request_id, request, uri):
    return request

def proxenet_response_hook(response_id, response, uri):
    return response

if __name__ == "__main__":
    # use for test cases
    pass
```


## Example

```python

AUTHOR = "hugsy"
PLUGIN_NAME = "AddHeader"


def proxenet_request_hook(request_id, request, uri):
    header = "X-Powered-By: python-proxenet"
    return request.replace("\r\n\r\n", + "\r\n"+header+"\r\n\r\n")

def proxenet_response_hook(response_id, response, uri):
    return response

if __name__ == "__main__":
    # use for test cases
    pass
```

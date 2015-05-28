# C plugin

This page will explain how to write a C plugin for `proxenet`.


## Plugin skeleton


```c
#include <sys/types.h>

typedef struct info {
    const char* AUTHOR;
    const char* PLUGIN_NAME;
} c_plugin_info_t;

c_plugin_info_t MyPlugin = {
    .AUTHOR = "",
    .PLUGIN_NAME = ""
};


char* proxenet_request_hook(unsigned long request_id, char *request, char* uri, size_t* buflen)
{
	return request;
}


char* proxenet_response_hook(unsigned long response_id, char *response, char* uri, size_t* buflen)
{
	return response;
}


int main(int argc, char** argv, char** envp)
{
	return 0;
}
```
_Note_: As you may have noticed, the definitions for the functions
`proxenet_request_hook()` and `proxenet_response_hook()` have an extra
argument. This argument, `buflen` is a pointer to a `size_t` variable, which
will contain the size of the request. Every plugin that modifies the length of
the request/response **MUST** update this value. See the example below for
demonstration.

`proxenet` does not recognize plain C files so it should be compiled first as a
[shared object](http://tldp.org/HOWTO/Program-Library-HOWTO/shared-libraries.html). The
following command might just do the trick:

```bash
$ cc -Wall -Werror -fPIC -shared -o MyPlugin.so MyPlugin.c
```

## Example

```c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

typedef struct info {
                const char* AUTHOR;
                const char* PLUGIN_NAME;
} c_plugin_info_t;


c_plugin_info_t MyPlugin = {
    .AUTHOR = "hugsy",
    .PLUGIN_NAME = "AddHeader"
};


char* proxenet_request_hook(unsigned long request_id, char *request, char* uri, size_t* buflen)
{
    const char* header = "X-Powered-By: c-proxenet\r\n\r\n";
	char* newReq = malloc(*buflen + strlen(header) + 2);
    memcpy(newReq, request, *buflen-2);
    memcpy(newReq + (*buflen-2), header, strlen(header));
    free(request);
    *buflen = *buflen + strlen(header) + 2;
	return newReq;
}


char* proxenet_response_hook(unsigned long response_id, char *response, char* uri, size_t* buflen)
{
	return response;
}


int main(int argc, char** argv, char** envp)
{
	return 0;
}
```

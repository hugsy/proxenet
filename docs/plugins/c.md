# C plugin

This page will explain how to write a C plugin for `proxenet`.


## Plugin skeleton

```c

struct info {
    const char* AUTHOR;
    const char* PLUGIN_NAME;
};

struct info MyPlugin = {
    .AUTHOR = "";
    .PLUGIN_NAME = "";
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


## Example

```c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct info {
    const char* AUTHOR;
    const char* PLUGIN_NAME;
};

struct info AddHeader = {
    .AUTHOR = "hugsy";
    .PLUGIN_NAME = "AddHeader";
};


char* proxenet_request_hook(unsigned long request_id, char *request, char* uri, size_t* buflen)
{
    const char* header = "X-Powered-By: c-proxenet\r\n\r\n";
	char* newReq = malloc(*buflen + strlen(header) + 2);
    memcpy(newReq, request, *buflen-2);
    memcpy(newReq + (*buflen-2), header, strlen(header));
    free(request);
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

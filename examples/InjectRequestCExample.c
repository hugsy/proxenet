/**
 *
 * Compile your C plugin with
 * $ cc -Wall -o 9MyPlugin.so -fPIC -shared 9MyPlugin.c
 * and move the shared object file to proxenet plugins directory
 *
 */
#include <stdio.h>

#define PLUGIN_NAME "HelloC"
#define PLUGIN_AUTHOR "@_hugsy_"


char* proxenet_request_hook(unsigned long request_id, char *request, char* uri, size_t* buflen)
{
	printf("Hello from C request hook, rid=%lu: %s\n", request_id, uri);
	return request;
}


char* proxenet_response_hook(unsigned long response_id, char *response, char* uri, size_t* buflen)
{
	printf("Hello from C response hook, rid=%lu: %s\n", response_id, uri);
	return response;
}


int main(int argc, char** argv, char** envp)
{
	return 0;
}

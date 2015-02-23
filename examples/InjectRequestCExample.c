/**
 *
 * Compile your C plugin with
 * $ cc -Wall -fPIC -shared 9MyPlugin.c -o 9MyPlugin.so
 * and move the shared object file to proxenet plugins directory
 *
 */
#include <stdio.h>

#define PLUGIN_NAME "HelloC"
#define PLUGIN_AUTHOR "@_hugsy_"


char* proxenet_request_hook(unsigned long request_id, char *request, char* uri)
{
	printf("Hello from C hook, rid=%lu: %s\n", request_id, uri);
	return request;
}


char* proxenet_response_hook(unsigned long response_id, char *response, char* uri)
{
	printf("Hello from C hook, rid=%lu: %s\n", response_id, uri);
	return response;
}


int main(int argc, char** argv, char** envp)
{
	return 0;
}

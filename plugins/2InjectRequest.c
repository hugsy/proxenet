/*
 * 
 * compile any C plugin with
 * $ cc -Wall -fPIC -c 9MyPlugin.c && gcc -shared -o 9MyPlugin.so 9MyPlugin.o
 *
 */
#include <stdio.h>

char* proxenet_request_hook(unsigned long request_id, char *request)
{
	printf("Hello from C hook, request-%lu\n", request_id);
	return request;
}


char* proxenet_response_hook(unsigned long response_id, char *response)
{
	printf("Hello from C hook, response-%lu\n", response_id);
	return response;
}


int main(int argc, char** argv, char** envp)
{
	return 0;
}

#include "stdio.h"
#include "stdlib.h"
#include <string.h>

int main()
{
	char *data;
	data = getenv("QUERY_STRING");

	puts("Content-type:text/html\r\n\r\n");
	puts(data);
	printf("Hello cgi!");

	return 0;
}

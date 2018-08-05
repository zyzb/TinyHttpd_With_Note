#include "stdio.h"
#include "stdlib.h"
#include <string.h>

int main()
{
	char *length;
	length = getenv("CONTENT_LENGTH");

	int temp = atoi(length);
	char data[1024];
	fgets(data, temp+1, stdin);

	puts("Content-type:text/html\r\n\r\n");
	puts("Length = ");
	puts(length);
	puts(data);
	

	printf("Hello cgi!");

	return 0;
}

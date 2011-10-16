#include <stdio.h>
#include <stdlib.h>

#include "getxiadstdagbyname.h"

int main(int argc, char *argv[])
{
	char result[512];
	char input[512];
	int retval;

	printf("Enter hostname for XIA destination DAG lookup (C-c to quit)\n");
	while (1)
	{
		printf("> ");
		scanf("%s", input);
		if ((retval = getxiadstdagbyname(result, input, "127.0.0.1")) == 0)
		{
			printf("%s DAG: %s\n", input, result);
		}
		else if (retval == -1)
		{
			printf("not found\n");	
		}
		else
		{
			printf("dns server connection failure\n");
			exit(0);
		}
	}
	return 0;	
}

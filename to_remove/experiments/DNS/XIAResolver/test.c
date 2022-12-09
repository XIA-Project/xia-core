#include <stdio.h>
#include <stdlib.h>

#include "XgetDAGbyname.h"

int main(int argc, char *argv[])
{
	char input[512];

	printf("Enter hostname for XIA destination DAG lookup (C-c to quit)\n");
	while (1)
	{
		printf("> ");
		scanf("%s", input);
    const char *tmp;
    tmp = XgetDAGbyname(input);
    if (tmp) {
      printf(" - %s\n", tmp);
    }
		else
		{
			printf("not found\n");	
		}
	}
	return 0;	
}

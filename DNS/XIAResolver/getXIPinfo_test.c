#include <stdio.h>
#include <stdlib.h>

#include "getXIPinfo.h"

int main(int argc, char *argv[])
{
	char input[512];
	int retval;
	XIP_info_t *XIPs = NULL;
	XIP_info_t *tmp;

	printf("Enter hostname for XIA destination DAG lookup (C-c to quit)\n");
	while (1)
	{
		printf("> ");
		scanf("%s", input);
		if ((retval = getXIPinfo(&XIPs, input)) > 0)
		{
			printf("found %d DAGs for %s\n", retval, input);
			tmp = XIPs;
			while (tmp != NULL)
			{
				printf(" - %s\n", tmp->dag);
				tmp = tmp->next;
			}
			freeXIPinfo(&XIPs);
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

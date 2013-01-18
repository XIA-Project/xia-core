#include <stdio.h>

#include "XregisterName.h"


void get_line(char *line, int len)
{
    unsigned int i;
    
    if (line == NULL)
        return;

    for (i = 0; i < len; i++) {
        line[i] = fgetc(stdin);
        if (line[i] == '\n') {
            line[i] = '\0';
            return;
        }
    }
    /* Ended prematurely to prevent buffer overflow */
    line[0] = '\0';
    printf("Input too long.\n");
}

/* Determine where DAG starts and hostname ends. Edits in_str so that hostname
 * ends with an '\0' */
char *dag_location(char *in_str)
{
    unsigned int i;
    for (i = 0; in_str[i] != '\0'; i++) {
        if (in_str[i] == ' ') {
            in_str[i] = '\0';
            return in_str + i + 1;
        }
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    char input[512];
    char *dag_str;

    printf("Enter hostname, DAG pair separated by a space (C-c to quit)\n");
    while (1) {
        /* Accept input */
        printf("> ");
        get_line(input, 512);
        dag_str = dag_location(input);
        if (dag_str == NULL) {
            printf("Invalid. Enter hostname DAG pair separated by a space.\n");
            continue;
        }

        /* Attempt to register name */
        if (XregisterNameBIND(input, dag_str) == 0)
            printf(" - Name successfully registered.\n");
        else
            printf(" - Name couldn't be registered.\n");
    }

    return 0;
}


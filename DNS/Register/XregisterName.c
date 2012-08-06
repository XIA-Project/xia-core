#include <stdio.h>

#include "Xsocket.h"

#define TTL 86400
#define NSUPDATE_COMM "nsupdate"
#define COMM_STRLEN 512

int XregisterNameBIND(char *name, char *dag)
{
    FILE *tmp;
    char tmp_name[L_tmpnam];
    char *zone;
    char comm_str[COMM_STRLEN];
    int ret;

    /* Interpret anything after the first dot to be a zone */
    for (zone = name; *zone != '\0' && *zone != '.'; zone += 1)
        ;
    if (*zone == '.') {
        /* Note whatever's after the first dot as a zone */
        zone += 1;
    }
    if (*zone == '\0') {
        /* Did not find a valid zone, point to beginning */
        zone = name;
    }

    /* Write to temporary file to send to nsupdate */
    if (tmpnam(tmp_name) == NULL) {
        fprintf(stderr, "Temporary file name could not be generated\n");
        return -1;
    }

    tmp = fopen(tmp_name, "w");
    if (tmp == NULL) {
        fprintf(stderr, "Temporary file could not be opened\n");
        return -1;
    }

    fprintf(tmp, "zone %s\nupdate add %s %d XIA %s\nsend\n", zone, name, TTL,
            dag);
    fflush(tmp);
    printf("zone %s\nupdate add %s %d XIA %s\nsend (filename=%s)\n", zone, name, TTL, dag, tmp_name);
    
    /* Run nsupdate to add the record */
    sprintf(comm_str, "%s %s", NSUPDATE_COMM, tmp_name);
    ret = -1;
    if (system(comm_str) == 0)
        ret = 0;

    fclose(tmp);
    unlink(tmp_name);

    return ret;
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "XgetDAGbyname.h"

static char DAG[256];

/*
 * get xia destination DAG
 * returns
 * DAG if successful, NULL otherwise
 */
const char *XgetDAGbyname(char *name) {
  char line[512];
  char *linend;
  
  // look for an hosts_xia file locally
  FILE *hostsfp = fopen(ETC_HOSTS, "r");
  int answer_found = 0;
  if (hostsfp != NULL) {
    while (fgets(line, 511, hostsfp) != NULL) {
      linend = line+strlen(line)-1;
      while (*linend == '\r' || *linend == '\n' || *linend == '\0') {
	linend--; 
      }
      *(linend+1) = '\0';
      if (line[0] == '#') {
	continue;
      } else if (!strncmp(line, name, strlen(name))
					   && line[strlen(name)] == ' ') {
	strncpy(DAG, line+strlen(name)+1, strlen(line)-strlen(name)-1);
	DAG[strlen(line)-strlen(name)-1] = '\0';
        answer_found = 1;
      }
		}
    fclose(hostsfp);
    if (answer_found) {
      return DAG;
    }
  }
  
  printf("Name not found in ./hosts_xia\n");
  return NULL;
}

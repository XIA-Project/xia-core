#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "Xsocket.h"
#include "dagaddr.hpp"
#include <assert.h>

#include <sys/time.h>

#include "prefetch_utils.h"

int verbose = 1;

/*
** write the message to stdout unless in quiet mode
*/
void say(const char *fmt, ...) {
	if (verbose) {
		va_list args;

		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}
}

/*
** always write the message to stdout
*/
void warn(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
}

/*
** write the message to stdout, and exit the app
*/
void die(int ecode, const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
	fprintf(stdout, "Exiting\n");
	exit(ecode);
}

char** str_split(char* a_str, const char *a_delim) {
	char** result = 0;
	int count = 0;
	int str_len = strlen(a_str);
	int del_len = strlen(a_delim);
	int i = 0;
	int j = 0;
	char* last_delim = 0;

	/* Count how many elements will be extracted. */
	for(i = 0 ; i < str_len; i++) 
		for(j = 0 ; j < del_len; j++) 
			if( a_str[i] == a_delim[j]){
				count++;
				last_delim = &a_str[i];
			}
	 /* Add space for trailing token. */
	count += last_delim < (a_str + strlen(a_str) - 1);
	
 	/* Add space for terminating null string so caller
			knows where the list of returned strings ends. */
 	count++;

	result = (char **) malloc(sizeof(char*) * count);
	
	// printf ("Splitting string \"%s\" into %i tokens:\n", a_str, count);
	
	i = 0;
	result[i] = strtok(a_str, a_delim);
	// printf ("%s\n",result[i]);
	
	for(i = 1; i < count; i++) {
		result[i] = strtok (NULL, a_delim);
		// printf ("%s\n",result[i]);
	}

	return result;
}


int sendCmd(int sock, const char *cmd) {
	int n;
	warn("Sending Command: %s \n", cmd);

	if ((n = Xsend(sock, cmd,  strlen(cmd), 0)) < 0) {
		Xclose(sock);
		 die(-1, "Unable to communicate\n");
	}

	return n;
}
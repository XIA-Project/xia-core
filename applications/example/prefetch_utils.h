#ifndef PREFETCH_UTILS_FILE
#define PREFETCH_UTILS_FILE

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

/*
** write the message to stdout unless in quiet mode
*/
void say(const char *fmt, ...);

/*
** always write the message to stdout
*/
void warn(const char *fmt, ...);
/*
** write the message to stdout, and exit the app
*/
void die(int ecode, const char *fmt, ...);

char** str_split(char* a_str, const char *a_delim);

int sendCmd(int sock, const char *cmd);

#endif
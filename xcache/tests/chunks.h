#ifndef __CHUNKS_H__
#define __CHUNKS_H__
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "dagaddr.hpp"

#define MAX_XID_SIZE 100

#define CHUNKS_SID "SID:00000000dd41b924c1001cfa1e1117a812492434"

#define CHUNKS_NAME "www_s.chunks.aaa.xia"

/*
** write the message to stdout unless in quiet mode
*/
static void __attribute__((unused)) say(const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

/*
** always write the message to stdout
*/
static void __attribute__((unused)) warn(const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  vfprintf(stdout, fmt, args);
  va_end(args);
}

/*
** write the message to stdout, and exit the app
*/
static void __attribute__((unused)) die(int ecode, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  vfprintf(stdout, fmt, args);
  va_end(args);
  fprintf(stdout, "exiting\n");
  exit(ecode);
}

#endif

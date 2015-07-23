#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include "logger.h"

struct logger_config logger_config;

void log(const char *func, int line, const char *fmt, va_list ap)
{
	char timebuf[512];
	time_t current_time = time(NULL);

	ctime_r(&current_time, timebuf);
	timebuf[strlen(timebuf) - 2] = 0;
	fprintf(stderr, "[%d, %s, %s, %d]: ", getpid(), timebuf, func, line);
	vfprintf(stderr, fmt, ap);
}

void configure_logger(struct logger_config *c)
{
	logger_config = *c;
}

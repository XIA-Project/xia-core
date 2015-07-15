#include <stdarg.h>
#include <stdio.h>
#include "logger.h"

int level = LOG_ERROR;

static struct logger_config config;

void log(const char *func, int line, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	fprintf(stderr, "%s:%d ", func, line);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");

	va_end(ap);
}

void configure_logger(struct logger_config *c)
{
	config = *c;
}

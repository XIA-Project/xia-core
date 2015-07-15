#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <iostream>


struct logger_config {
	int level;
	int log_to_stdout;
	int log_to_syslog;
	int log_to_file;
	char filename[256];
};

enum {
	LOG_ERROR,
	LOG_DEBUG,
	LOG_WARN,
	LOG_INFO,
};

/* Basic logging macro */
#define LOG(_level, fmt, ...)								\
	do {													\
		if(_level <= level)									\
			log(__func__, __LINE__, fmt, ##__VA_ARGS__);	\
	} while(0)

/* More convenient macros */
#define LOG_ERROR(...) LOG(LOG_ERROR, __VA_ARGS__)
#define LOG_DEBUG(...) LOG(LOG_DEBUG, __VA_ARGS__)
#define LOG_WARN(...) LOG(LOG_WARN, __VA_ARGS__)
#define LOG_INFO(...) LOG(LOG_INFO, __VA_ARGS__)

/* Current log level */
extern int level;

/* Configure logging */
void configure_logger(struct logger_config *);

/* Actuall logging function */
void log(const char *func, int line, const char *fmt, ...);

#endif

#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <iostream>


struct logger_config {
	int level;
	int mask;
	int log_to_stdout;
	int log_to_syslog;
	int log_to_file;
	char filename[256];
};

/* Logging levels */
enum {
	LOG_ERROR,
	LOG_DEBUG,
	LOG_WARN,
	LOG_INFO,
};

/* Logging modules */
#define LOG_NONE 0x0
#define LOG_ALL 0xFFFF

enum {
	LOG_CTRL,
	LOG_CACHE,
	LOG_SLICE,
	LOG_META,
	LOG_STORE,
	LOG_POLICIES,
	LOG_XCACHE,
	LOG_END,
};

#include <stdarg.h>

/* Actuall logging function */
void log(const char *func, int line, const char *fmt, va_list ap);

/* Basic logging macro */
#define LOG(__mask, __level, __fmt, __ap)			\
	do {							\
		if((__level <= logger_config.level) &&		\
		   ((__mask & logger_config.mask) != 0))	\
			log(__func__, __LINE__, __fmt, __ap);	\
	} while(0)

/* More convenient macros */
#define LOG_INFO(__mask, __fmt, __ap) LOG(__mask, LOG_INFO, __fmt, __ap)
#define LOG_WARN(__mask, __fmt, __ap) LOG(__mask, LOG_WARN, __fmt, __ap)
#define LOG_DEBUG(__mask, __fmt, __ap) LOG(__mask, LOG_DEBUG, __fmt, __ap)
#define LOG_ERROR(__mask, __fmt, __ap) LOG(__mask, LOG_ERROR, __fmt, __ap)

/* Configure logging */
void configure_logger(struct logger_config *);

extern struct logger_config logger_config;

#define DEFINE_LOG_MACROS(__module)						\
	static inline void LOG_##__module##_INFO(const char *fmt, ...) {	\
		va_list ap;							\
		va_start(ap, fmt);						\
		LOG_INFO(1 << LOG_##__module, fmt, ap);				\
		va_end(ap);							\
	}									\
										\
	static inline void LOG_##__module##_WARN(const char *fmt, ...) {	\
		va_list ap;							\
		va_start(ap, fmt);						\
		LOG_WARN(1 << LOG_##__module, fmt, ap);				\
		va_end(ap);							\
	}									\
										\
	static inline void LOG_##__module##_DEBUG(const char *fmt, ...) {	\
		va_list ap;							\
		va_start(ap, fmt);						\
		LOG_DEBUG(1 << LOG_##__module, fmt, ap);			\
		va_end(ap);							\
	}									\
										\
	static inline void LOG_##__module##_ERROR(const char *fmt, ...) {	\
		va_list ap;							\
		va_start(ap, fmt);						\
		LOG_ERROR(1 << LOG_##__module, fmt, ap);			\
		va_end(ap);							\
	}

#endif

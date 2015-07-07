#ifndef __LOGGER_H__
#define __LOGGER_H__
#include <stdarg.h>
#include <stdio.h>
#include <iostream>

enum {
  LOG_INFO,
  LOG_DEBUG,
  LOG_ERR,
};

/**
 * Logger:
 * TODO Add support for logging to file
 */
class Logger {
private:
  std::string filename, prefix;
  bool logToFile, logToStdout;
  int currentLevel;

public:
  Logger();
  void setLogFile(std::string filename);
  int log(int, char *fmt, ...);
  void setLogLevel(int level);
  
};

#endif

#include <stdio.h>
#include <stdarg.h>
#include "trace.hh"


CLICK_DECLS
/* Class variables */
int Trace::traceLevel = 0;

/* Returns the current trace level */
int Trace::getLevel() {
  return Trace::traceLevel;
}

/* Sets the current trace level. 
    Returns: The previous level   */
int Trace::setLevel(int level) {
  int ret = Trace::traceLevel;
  Trace::traceLevel = level;
  return ret;
}

/* Prints a formatted message if the current trace level is >= trace level */
void Trace::print(const int level, const char *fmt, ...) {
  va_list argList;
  va_start(argList, fmt);

  if (Trace::traceLevel >= level) {
    vprintf(fmt, argList);
  }
  va_end(argList);
}



CLICK_ENDDECLS
ELEMENT_PROVIDES(Trace)

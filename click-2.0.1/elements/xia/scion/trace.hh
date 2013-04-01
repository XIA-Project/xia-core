#ifndef _TRACE_H
#define _TRACE_H

#include <click/config.h>

/* Provides debugging facilities */
class Trace {

public:
  /* Some common trace level values */
#ifdef _DEBUG
  // Don't want a screenful of stats
  static const int STATS      = 10000; 
#else
  static const int STATS      = -10;    // We always print stats!
#endif 
  static const int OFF        =   0;
  static const int ERR        =  50;
  static const int WARNING    = 100;  
  static const int INFO       = 150;
  static const int MILESTONES = 200;

  /* Returns the current trace level */
  static int getLevel();

  /* Sets the current trace level. 
     Returns: The previous level   */
  static int setLevel(int level);

  /* Prints a formatted message if the current trace level is >= trace level */
  static void print(const int level, const char *fmt, ...);

private:
  /* Current tracing level */
  static int traceLevel;

};
#endif

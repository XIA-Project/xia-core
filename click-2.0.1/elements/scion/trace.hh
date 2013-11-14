/*
** Copyright 2013 Carnegie Mellon University / ETH Zurich
** 
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
** 
** http://www.apache.org/licenses/LICENSE-2.0 
** 
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

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

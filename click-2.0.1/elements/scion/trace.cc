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

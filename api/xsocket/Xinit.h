/*
** Copyright 2011 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef XINIT_H
#define XINIT_H

#include "xia.pb.h"

//Click side: Control/data address/info
#define DEFAULT_CLICKPORT "1500"

//set xia.click sorter to sort based on these ports. 


#define __PORT_LEN 6

class  __InitXSocket {
    public:
		~__InitXSocket() {};
		__InitXSocket();
        static void read_conf(const char *inifile, const char *section_name);
	    static void print_conf();
	private:
	static __InitXSocket _instance;
};

struct __XSocketConf {
  static int initialized;
  char click_port[__PORT_LEN];
};

extern struct __XSocketConf _conf;
extern struct __XSocketConf* get_conf(void);

#define CLICKPORT  (get_conf()->click_port)
#endif

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

#ifndef APIINIT_H
#define APIINIT_H


#define DEFAULT_PROCPORT "1989"


#define __PORT_LEN 6

class  __InitSession {
    public:
		~__InitSession() {};
		__InitSession();
        static void read_session_conf(const char *inifile, const char *section_name);
	    static void print_session_conf();
	private:
	static __InitSession _instance;
};

struct __SessionConf {
  static int initialized;
  char proc_port[__PORT_LEN];
};

extern struct __SessionConf _session_conf;
extern struct __SessionConf* get_session_conf(void);

#define PROCPORT  (atoi(get_session_conf()->proc_port))
#endif

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

//Socket library side: Control address/info
#define DEFAULT_MYADDRESS "172.0.0.1" 
#define MYPORT "0"//Chooses random port

//Click side: Control/data address/info
//The actual IPs don't matter, it just has to be in the correct subnet
#define DEFAULT_CLICKCONTROLADDRESS "172.0.0.2" 
#define CLICKCONTROLPORT "5001"
//#define CLICKOPENPORT "5001"
#define CLICKBINDPORT "5002"
#define CLICKCLOSEPORT "5003"
#define CLICKCONNECTPORT "5004"
#define CLICKACCEPTPORT "5005"


#define DEFAULT_CLICKDATAADDRESS "172.0.0.2" 
#define CLICKDATAPORT "10000"
#define CLICKPUTCIDPORT "10002"
#define CLICKSENDTOPORT "10001"


//set xia.click sorter to sort based on these ports. 


#define __IP_ADDR_LEN 64

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
	char api_addr[__IP_ADDR_LEN];
	char click_dataaddr[__IP_ADDR_LEN];
	char click_controladdr[__IP_ADDR_LEN];
};

extern struct __XSocketConf _conf;
extern struct __XSocketConf* get_conf(void);

#define MYADDRESS (get_conf()->api_addr)
#define CLICKCONTROLADDRESS  (get_conf()->click_controladdr)
#define CLICKDATAADDRESS (get_conf()->click_dataaddr)
#endif

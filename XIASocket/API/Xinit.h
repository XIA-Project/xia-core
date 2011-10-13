#ifndef XINIT_H
#define XINIT_H

#include "xia.pb.h"

//Socket library side: Control address/info
#define DEFAULT_MYADDRESS "192.0.0.1" 
#define MYPORT "0"//Chooses random port

//Click side: Control/data address/info
//The actual IPs don't matter, it just has to be in the correct subnet
#define DEFAULT_CLICKCONTROLADDRESS "192.0.0.2" 
#define CLICKCONTROLPORT "5001"
//#define CLICKOPENPORT "5001"
//#define CLICKBINDPORT "5002"
#define CLICKCLOSEPORT "5003"
//#define CLICKCONNECTPORT "5004"
//#define CLICKACCEPTPORT "5005"


#define DEFAULT_CLICKDATAADDRESS "192.0.0.2" 
#define CLICKDATAPORT "10000"
//#define CLICKPUTCIDPORT "10002"
//#define CLICKSENDTOPORT "10001"


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

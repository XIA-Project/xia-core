#ifndef XINIT_H
#define XINIT_H
#define __IP_ADDR_LEN 64

struct __InitXSocket {
	__InitXSocket();
	void print_conf();
};

struct __XSocketConf {
	char api_addr[__IP_ADDR_LEN];
	char click_dataaddr[__IP_ADDR_LEN];
	char click_controladdr[__IP_ADDR_LEN];
};

extern struct __XSocketConf _conf;

#define MYADDRESS (_conf.api_addr)
#define CLICKCONTROLADDRESS  (_conf.click_controladdr)
#define CLICKDATAADDRESS (_conf.click_dataaddr)
#endif

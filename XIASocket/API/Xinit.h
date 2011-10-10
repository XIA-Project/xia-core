#ifndef XINIT_H
#define XINIT_H
#define __IP_ADDR_LEN 64

class  __InitXSocket {
    public:
		~__InitXSocket() {};
		__InitXSocket();
	private:
	void print_conf();
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

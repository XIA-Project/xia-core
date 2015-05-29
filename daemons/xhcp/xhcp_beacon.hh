#ifndef __XHCP_BEACON_H__
#define __XHCP_BEACON_H__
#include <string>

class XHCPBeacon {
	public:
		XHCPBeacon(
		        std::string ad="", std::string rhid="",
		        std::string r4id="", std::string ns_dag="");
		XHCPBeacon(char *buf);
		~XHCPBeacon();
		std::string getAD();
		std::string getRouterHID();
		std::string getRouter4ID();
		std::string getNameServerDAG();
		void setAD(std::string);
		void setRouterHID(std::string);
		void setRouter4ID(std::string);
		void setNameServerDAG(std::string);
		bool operator==(const XHCPBeacon& other);
	private:
		std::string _ad;
		std::string _router_hid;
		std::string _router_4id;
		std::string _nameserver_dag;
};

#endif

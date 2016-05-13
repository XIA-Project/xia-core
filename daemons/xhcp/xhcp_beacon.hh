#ifndef __XHCP_BEACON_H__
#define __XHCP_BEACON_H__
#include <string>

class XHCPBeacon {
	public:
		XHCPBeacon(std::string dag="", std::string rhid="", std::string r4id="", std::string ns_dag="", std::string rvc_dag="");
		XHCPBeacon(char *buf);
		~XHCPBeacon();
		std::string getRouterDAG();
		std::string getRouterHID();
		std::string getRouter4ID();
		std::string getNameServerDAG();
		std::string getRendezvousControlDAG();
		void setRouterDAG(std::string);
		void setRouter4ID(std::string);
		void setNameServerDAG(std::string);
		void setRendezvousControlDAG(std::string);
		bool operator==(const XHCPBeacon& other);
		//std::string to_string() { return _dag + "," + _router_4id + "," + _nameserver_dag; }
		std::string to_string();
	private:
		std::string _dag;
		std::string _router_hid;
		std::string _router_4id;
		std::string _nameserver_dag;
		std::string _rv_control_dag;
};

#endif

#ifndef __XHCP_INTERFACE_H__
#define __XHCP_INTERFACE_H__
#include <string>

class XHCPInterface {
	public:
		XHCPInterface(int id=-1, std::string name="",
		        std::string hid="", std::string ad="", std::string rhid="",
		        std::string r4id="", std::string ns_dag="");
		//XHCPInterface(int id, std::string name, std::string hid, std::string ad, std::string rhid, std::string r4id, std::string ns_dag);
		~XHCPInterface();
		int getID();
		std::string getName();
		std::string getHID();
		std::string getAD();
		std::string getRouterHID();
		std::string getRouter4ID();
		std::string getNameServerDAG();
		void setID(int);
		void setName(std::string);
		void setHID(std::string);
		void setAD(std::string);
		void setRouterHID(std::string);
		void setRouter4ID(std::string);
		void setNameServerDAG(std::string);
		bool isActive();
		bool operator==(const XHCPInterface& other);
	private:
		int _id;
		std::string _name;
		std::string _hid;
		std::string _ad;
		std::string _router_hid;
		std::string _router_4id;
		std::string _nameserver_dag;
};

#endif

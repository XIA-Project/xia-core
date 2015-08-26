#ifndef __XHCP_INTERFACE_H__
#define __XHCP_INTERFACE_H__
#include <string>

class XHCPInterface {
	public:
		XHCPInterface(int id=-1, bool active=false, std::string name="",
		        std::string hid="", std::string rdag="", std::string rhid="",
		        std::string r4id="", std::string ns_dag="",
				std::string rvc_dag="");
		//XHCPInterface(int id, std::string name, std::string hid, std::string ndag, std::string rhid, std::string r4id, std::string ns_dag);
		~XHCPInterface();
		int getID();
		std::string getName();
		std::string getHID();
		std::string getRouterDAG();
		std::string getRouterHID();
		std::string getRouter4ID();
		std::string getNameServerDAG();
		std::string getRendezvousControlDAG();
		void setID(int);
		void setActive() { _active = true;}
		void setName(std::string);
		void setHID(std::string);
		void setRouterDAG(std::string);
		void setRouterHID(std::string);
		void setRouter4ID(std::string);
		void setNameServerDAG(std::string);
		void setRendezvousControlDAG(std::string);
		bool hasRendezvousControlDAG();
		bool isActive();
		bool operator==(const XHCPInterface& other);
	private:
		int _id;
		bool _active; // Interfaces turn active after receiving a beacon
		std::string _name;
		std::string _hid;
		std::string _router_dag;
		std::string _router_hid;
		std::string _router_4id;
		std::string _nameserver_dag;
		std::string _rv_control_dag;
};

#endif

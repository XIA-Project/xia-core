#ifndef _XGNS_H
#define _XGNS_H

#include <cpr/cpr.h>

#define GNSHTTPPROXY_PORT 5678

class GNSServer {
	public:
		GNSServer(std::string publisher_name);
		~GNSServer();
		bool makeEntry(std::string name, std::string http_dag_string);
		bool makeTempEntry(std::string name, std::string http_dag_string);
		bool removeEntry(std::string name);
	private:
		std::vector<std::string> _temp_entries;
		std::string _guid;
		std::string _gns_url;
};

GNSServer::GNSServer(std::string publisher_name)
{
	_gns_url = "localhost:" + std::to_string(GNSHTTPPROXY_PORT) + "/GNS/";
	// Try connecting to server and getting the GUID
	std::string cmd = _gns_url + "lookupguid?";
	cmd += "name=" + publisher_name;
	auto response = cpr::Get(cpr::Url{cmd});
	std::cout << "Response status code: " << response.status_code << std::endl;
	if(response.status_code != 200) {
		throw std::runtime_error("ERROR connecting to GNSHTTPProxy on 5678");
	}
	std::cout << response.text << std::endl;
	_guid = response.text;
}

GNSServer::~GNSServer()
{
	for(auto entry : _temp_entries) {
		removeEntry(entry);
	}
}

bool GNSServer::makeEntry(std::string name, std::string http_dag_string)
{
	std::string cmd = _gns_url + "create?";
	cmd += "guid=" + _guid;
	cmd += "&field=" + name;
	cmd += "&value=" + http_dag_string;
	std::cout << "Command: " << cmd << std::endl;
	auto response = cpr::Get(cpr::Url{cmd});
	if(response.status_code != 200) {
		return false;
	}
	std::cout << response.text << std::endl;
	return true;
}

bool GNSServer::makeTempEntry(std::string name, std::string http_dag_string)
{
	if(makeEntry(name, http_dag_string) == false) {
		return false;
	}
	_temp_entries.push_back(name);
	return true;
}

bool GNSServer::removeEntry(std::string name)
{
	std::string cmd = _gns_url + "removefield?";
	cmd += "guid=" + _guid;
	cmd += "&field=" + name;
	std::cout << "Remove Command: " << cmd << std::endl;
	auto response = cpr::Get(cpr::Url(cmd));
	if(response.status_code != 200) {
		return false;
	}
	std::cout << response.text << std::endl;
	return true;
}

#endif // _XGNS_H

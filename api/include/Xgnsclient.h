#ifndef _XGNSCLIENT_H
#define _XGNSCLIENT_H

#include <cpr/cpr.h>
#include <json.hpp>

#include <fstream>

class GNSClient {
	public:
		GNSClient(std::string conf_file);
		std::vector<std::string> queryEntry(std::string name);
	private:
		std::string _guid;
		std::string _gns_url;
};

GNSClient::GNSClient(std::string conf_file)
{
	// Read in a config file for GNS server information
	std::ifstream i(conf_file);
	nlohmann::json j;
	i >> j;

	_gns_url = j["gnsserveraddr"].get<std::string>() + "/GNS/";
	// Try connecting to server and getting the GUID
	std::string cmd = _gns_url + "lookupguid?";
	cmd += "name=" + j["xiapublishername"].get<std::string>();
	auto response = cpr::Get(cpr::Url{cmd});
	std::cout << "Response status code: " << response.status_code << std::endl;
	if(response.status_code != 200) {
		throw std::runtime_error("ERROR connecting to GNS Server for queries");
	}
	// TODO: If response code is not OK, throw exception
	std::cout << response.text << std::endl;
	_guid = response.text;
}

std::vector<std::string> GNSClient::queryEntry(std::string name)
{
	std::vector<std::string> results;
	std::string cmd = _gns_url + "read?";
	cmd += "guid=" + _guid;
	cmd += "&field=" + name;
	std::cout << "Command: " << cmd << std::endl;
	auto response = cpr::Get(cpr::Url{cmd});
	if(response.status_code != 200) {
		std::cout << "ERROR invalid response from GNS query" << std::endl;
		return results;
	}
	std::cout << response.text << std::endl;
	auto json = nlohmann::json::parse(response.text);
	if(json.empty()) {
		std::cout << "ERROR no response from GNS query" << std::endl;
		return results;
	}
	for(nlohmann::json::iterator it=json.begin(); it!=json.end(); ++it) {
		std::string addr = it.value()[0].get<std::string>();
		std::cout << "Instance: " << it.key() << " Addr: " << addr << std::endl;
		results.push_back(addr);
	}
	return results;
}

#endif // _XGNSCLIENT_H

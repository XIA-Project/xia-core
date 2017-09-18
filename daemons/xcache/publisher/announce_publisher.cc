#include <iostream>
#include <string>
#include "Xsocket.h"
#include "xcache.h"		// XcacheHandle
#include "dagaddr.hpp"	// Graph

#include <sys/types.h>	// stat
#include <sys/stat.h>	// stat
#include <unistd.h>		// stat

void usage(char *progname)
{
	std::string appname(progname);
	std::cout << "Usage: " << appname << " <publisher_name>" << std::endl;
}
/*!
 * Upload a publisher certificate and announce the publisher
 */
int main(int argc, char **argv)
{
	struct stat statbuf;
	XcacheHandle xcache;
	sockaddr_x *addrs = NULL;
	int count;

	if(argc != 2) {
		usage(argv[0]);
		return -1;
	}

	// Build path to the certificate file
	char buf[2048];
	if(XrootDir(buf, sizeof(buf)) == NULL) {
		std::cout << "ERROR: cannot find root directory" << std::endl;
	}

	// Find the certificate for this publisher
	std::string publisher_name(argv[1]);
	std::string root_dir(buf);
	std::string cert_file = root_dir + "/publishers/" + publisher_name +
		"/" + publisher_name + ".cert";
	std::cout << "Certificate: " << cert_file << std::endl;

	// Ensure that the certificate exists
	if(stat(cert_file.c_str(), &statbuf)) {
		std::cout << "ERROR: failed checking status of cert file" << std::endl;
		return -1;
	}
	if(!S_ISREG(statbuf.st_mode)) {
		std::cout << "ERROR: Invalid cert file" << std::endl;
		return -1;
	}

	// Load cert file into xcache
	if(XcacheHandleInit(&xcache)) {
		std::cout << "ERROR: Unable to communicate with xcache" << std::endl;
		return -1;
	}
	if((count = XputFile(&xcache, cert_file.c_str(), 16384, &addrs)) < 0) {
		std::cout << "ERROR: Failed to put file into xcache" << std::endl;
		return -1;
	}
	if(count != 1) {
		std::cout << "ERROR: Cert was too big to fit in a chunk" << std::endl;
		return -1;
	}
	Graph g(&addrs[0]);
	std::string cert_dag = g.dag_string();
	std::cout << "Cert address is " << cert_dag << std::endl;

	// Announce location of cert file on nameserver so clients can get to it
	std::string cert_name = publisher_name + ".publisher.cert.xia";
	if(XregisterName(cert_name.c_str(), &addrs[0])) {
		std::cout << "ERROR: Failed registering " << cert_name <<
			" with the nameservice" << std::endl;
		return -1;
	}
	std::cout << "Successfully registered " << publisher_name << std::endl;
	return 0;
}

#include "publisher_key.h"
#include "publisher_key_mgr.h"
#include "publisher_key_mgmt.pb.h"
#include "publisher_list.h"
#include <xcache.h>

#include <future>		// std::future, std::async
#include <iostream>		// std::cout, std::endl

#include <sys/types.h>		// bind, listen, select, accept
#include <sys/socket.h>		// bind, listen, accept
#include <sys/stat.h>		// chmod
#include <sys/time.h>		// select
#include <sys/un.h>			// sockaddr_un
#include <unistd.h>			// select

PublisherKeyMgr::PublisherKeyMgr()
{
	// Use predefined unix domain socket name - see xcache.h
	std::string publisher_mgr_sock_name(PUBLISHER_MGR_SOCK_NAME);

	struct sockaddr_un manager_addr;
	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

	manager_addr.sun_family = AF_UNIX;
	remove(publisher_mgr_sock_name.c_str());
	strcpy(manager_addr.sun_path, publisher_mgr_sock_name.c_str());

	if (bind(sockfd, (struct sockaddr *)&manager_addr, sizeof(manager_addr))) {
		throw "Unable to bind to PublisherKeyMgr address";
	}
	if (chmod(publisher_mgr_sock_name.c_str(), 0x777)) {
		std::cout << "ERROR changing permissions for PublisherKeyMgr addr"
			<< " - need to run publisher key manager as root" << std::endl;
		throw "Unable to change permissions for PublisherKeyMgr sock";
	}
	if (listen(sockfd, 100)) {
		std::cout << "ERROR listening to PublisherKeyMgr requests" << std::endl;
		throw "Unable to listen for PublisherKeyMgr requests";
	}
	_sockfd = sockfd;
}

PublisherKeyMgr::~PublisherKeyMgr()
{
	if (close(_sockfd)) {
		std::cout << "ERROR closing PublisherKeyMgr socket" << std::endl;
	}
}

// Worker thread - runs asynchronously
// returns fd to be closed
int PublisherKeyMgr::process(int fd)
{
	std::cout << "Processing " << fd << std::endl;

	// Wait for up to 5 seconds for the application to send us data
	int retval;
	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	retval = select(fd+1, &rfds, NULL, NULL, &tv);
	if (retval == 0) {
		std::cout << "Client not sending command" << std::endl;
		std::cout << "Dropping connection" << std::endl;
		return fd;
	}
	if (retval == -1) {
		std::cout << "ERROR waiting for client to send cmd" << std::endl;
		return fd;
	}
	assert(retval == 1);	// only 1 descriptor that can be ready

	// Read command length from client
	uint32_t cmdlen;
	retval = recv(fd, &cmdlen, sizeof(cmdlen), 0);
	if (retval != sizeof(cmdlen)) {
		std::cout << "ERROR failed getting client cmd len" << std::endl;
		return fd;
	}

	// Now read the command
	char *command = (char *)calloc(1, cmdlen);
	if (command == NULL) {
		std::cout << "ERROR allocating memory for command" << std::endl;
		return fd;
	}
	retval = recv(fd, command, cmdlen, 0);
	if (retval != (int) cmdlen) {
		std::cout << "ERROR expected cmd bytes: " << cmdlen
			<< " got: " << retval << std::endl;
		return fd;
	}

	// Convert into command protobuf
	PublisherKeyCmdBuf cmd_buf;
	if(cmd_buf.ParseFromString(std::string(command, cmdlen)) == false) {
		std::cout << "ERROR parsing command from client" << std::endl;
		return fd;
	}

	if (cmd_buf.has_key_request()) {
		handle_key_request(fd, cmd_buf.key_request());
	} else if (cmd_buf.has_sign_request()) {
		handle_sign_request(fd, cmd_buf.sign_request());
	} else if (cmd_buf.has_verify_request()) {
		handle_verify_request(fd, cmd_buf.verify_request());
	}
	return fd;
}

void PublisherKeyMgr::send_response(int fd, PublisherKeyResponseBuf &resp)
{
	int retval;
	std::string response_str;
	resp.SerializeToString(&response_str);

	std::cout << "PublisherKeyMgr response size: " << response_str.size()
		<< " sent to socket " << fd << std::endl;
	uint32_t response_size = response_str.size();
	retval = send(fd, &response_size, sizeof(response_size), 0);
	if (retval != (int) sizeof(response_size)) {
		std::cout << "Unable to send response size" << std::endl;
		return;
	}

	retval = send(fd, response_str.c_str(), response_str.size(), 0);
	if (retval != (int) response_str.size()) {
		std::cout << "Entire response not sent to client." << std::endl;
		std::cout << "Response size: " << response_str.size()
			<< "Sent bytes: " << retval;
		return;
	}
}

void PublisherKeyMgr::handle_key_request(
		int fd, const PublisherKeyRequest &req)
{
	std::cout << "PublisherKeyMgr::handle_key_request called" << std::endl;
	PublisherList *publishers = PublisherList::get_publishers();
	PublisherKey *publisher = publishers->get(req.publisher_name());
	std::string keystr = publisher->pubkey();
	std::string cert_dag = publisher->cert_dag_str();

	PublisherKeyResponseBuf response_buf;
	PublisherKeyResponse *response = response_buf.mutable_key_response();

	// report status
	if (keystr.size() == 0) {
		response->set_success(false);
	} else {
		response->set_success(true);
		response->set_publisher_key(keystr);
		if(cert_dag.size() > 0) {
			response->set_cert_dag(cert_dag);
		}
	}
	send_response(fd, response_buf);
}

void PublisherKeyMgr::handle_verify_request(
		int fd, const PublisherVerifyRequest &req)
{
	std::cout << "PublisherKeyMgr::handle_verify_request called" << std::endl;
	PublisherList *publishers = PublisherList::get_publishers();
	PublisherKey *publisher = publishers->get(req.publisher_name());
	std::string signature = req.signature();
	std::string digest = req.digest();

	PublisherKeyResponseBuf response_buf;
	PublisherVerifyResponse *response = response_buf.mutable_verify_response();
	if (publisher->isValidSignature(digest, signature)) {
		response->set_success(true);
	} else {
		response->set_success(false);
	}
	send_response(fd, response_buf);
}

void PublisherKeyMgr::handle_sign_request(
		int fd, const PublisherSignRequest &req)
{
	std::cout << "PublisherKeyMgr::handle_sign_request called" << std::endl;
	bool success = false;

	PublisherList *publishers = PublisherList::get_publishers();
	PublisherKey *publisher = publishers->get(req.publisher_name());
	std::string digest = req.digest();

	std::string signature;
	if (publisher->sign(digest, signature)) {
		std::cout << "Failed to sign provided digest" << std::endl;
	} else {
		success = true;
	}

	PublisherKeyResponseBuf response_buf;
	PublisherSignResponse *response = response_buf.mutable_sign_response();

	response->set_success(success);
	if(success) {
		response->set_signature(signature);
	}
	send_response(fd, response_buf);
}

void PublisherKeyMgr::manage()
{
	fd_set rfds;
	struct timeval tv;
	int retval;
	int nfds;

	while (true) {
		FD_ZERO(&rfds);
		FD_SET(_sockfd, &rfds);
		nfds = _sockfd + 1;

		tv.tv_sec = 0;
		tv.tv_usec = 100000;	// 100 ms

		retval = select(nfds, &rfds, NULL, NULL, &tv);

		if (retval == -1) {
			std::cout << "ERROR waiting for connection" << std::endl;
			perror("select() for PublisherKeyMgr");
		}
		if (retval) {

			// Accept a connection on _sockfd
			int newfd = accept(_sockfd, NULL, NULL);
			if(newfd > 0) {

				// Submit task to find/fetch the key and cert_dag
				auto fut = std::async (std::launch::async,
						&PublisherKeyMgr::process, this, newfd);
				results.push_back(std::move(fut));
			}
		}

		// Now walk through all the futures to see if any are ready
		std::chrono::milliseconds zero(0);

		for (auto it = results.begin(); it!=results.end();) {

			if ((*it).wait_for(zero) == std::future_status::ready) {
				auto result = (*it).get();

				if (close(result)) {
					std::cout << "Failed closing socket "
						<< result << std::endl;
				}
				it = results.erase(it);
			} else {
				++it;
			}
		}
	}
}

int main()
{
	std::unique_ptr<PublisherKeyMgr> manager(new PublisherKeyMgr());
	manager->manage();
	return 0;
}

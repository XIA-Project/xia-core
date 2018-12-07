/* ts=4 */
/*
** Copyright 2011 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <XIAStreamSocket.hh>
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <future>

/* Demonstrating XIAStreamSocket usage with automatic cleanup.
 *
 * A client and server are created. The server accepts a client connection
 * and receives data from the client. All within 75 lines of code.
 * All sockets are closed and temporary keys are cleaned up automatically
 */

sockaddr_x servaddr;
std::atomic<bool> server_ready(false);

// The client thread
void client()
{
	// Prepare a socket and wait for the server to signal readiness
	XIAStreamSocket clientsock;
	while(server_ready.load() == false) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	// Server is ready for connections, connect once to it
	if(clientsock.connect(&servaddr)) {
		std::cout << "ERROR connect to server failed" << std::endl;
		// Failed connection, socket will be cleaned up automatically
		return;
	}

	std::string buf("Hello world!");
	int rc = clientsock.send(buf.c_str(), buf.size());

	if(rc != (int)buf.size()) {
		std::cout << "ERROR: client unable to send all data" << std::endl;
		return;
	}
	// The connected socket will be closed automatically
}

int main()
{
	// Launch client
	auto cfut = std::async(std::launch::async, client);

	// Now set up the server bound to a temporary SID
	XIAStreamSocket sock;
	if(sock.bind_and_listen_on_temp_sid(5, servaddr)) {
		throw std::runtime_error("ERROR binding to socket");
	}

	// Notify client that the server is ready
	server_ready.store(true);

	// Accepting a connection from the client
	sockaddr_x sa;
	auto newsock = sock.accept(&sa);

	char buf[64];
	bzero(buf, 64);
	int bytes = newsock->recv(buf, sizeof(buf));
	if(bytes <= 0) {
		std::cout << "ERROR: server did not get valid data" << std::endl;
		return -1;
	}
	std::string data(buf, bytes);
	std::cout << "Server got: " << data << std::endl;

	// Ensuring the client ends now
	cfut.get();

	// Both, the newly connected socket and bound socket are closed here
	return 0;
}

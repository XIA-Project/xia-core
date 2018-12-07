#ifndef _XIASTREAMSOCKET_HH
#define _XIASTREAMSOCKET_HH

// XIA Headers
#include <Xsocket.h>
#include <Xkeys.hh> // for SIDKey

// C++ headers
#include <iostream>
#include <memory>

class XIAStreamSocket {
	public:
		XIAStreamSocket();
		XIAStreamSocket(int valid_fd);
		~XIAStreamSocket();
		int fd(); // TODO: Make private in future. Xselect/Xpoll needs it
		int bind_and_listen_on_temp_sid(int num_conn, sockaddr_x &addr);
		int bind_and_listen(const sockaddr_x *addr, int num_conn);
		std::unique_ptr<XIAStreamSocket> accept(sockaddr_x *sa);
		int connect(const sockaddr_x *addr);
		int send(const void *buf, size_t len);
		int recv(void *buf, size_t len);

	private:
		int bind(const sockaddr_x *addr);
		int _fd = -1;
		std::unique_ptr<SIDKey> sid_key;
};

// Default constructor: Create a stream XIA socket
XIAStreamSocket::XIAStreamSocket()
{
	auto sockfd = Xsocket(AF_XIA, SOCK_STREAM, 0);
	if(sockfd < 0) {
		throw std::runtime_error("ERROR creating Xsocket");
	}
	_fd = sockfd;
}

// Convert provided XIA socket as an XIAStreamSocket - used by accept
XIAStreamSocket::XIAStreamSocket(int valid_fd)
{
	if(valid_fd < 0) {
		throw std::runtime_error("ERROR invalid fd");
	}
	_fd = valid_fd;
}

// Close the underlying socket when XIAStreamSocket instance goes out of scope
XIAStreamSocket::~XIAStreamSocket()
{
	if(_fd != -1) {
		std::cout << "XIAStreamSocket: Closing socket: " << _fd << std::endl;
		if(Xclose(_fd)) {
			std::cout << "XIAStreamSocket: ERROR closing sock"
				<< _fd << std::endl;;
		}
	}
}

// Bind given address to the underlying socket
int XIAStreamSocket::bind(const sockaddr_x *addr)
{
	return Xbind(_fd, (const sockaddr *)addr, sizeof(sockaddr_x));
}

// Bind given address to underlying socket and listen for incoming connections
int XIAStreamSocket::bind_and_listen(const sockaddr_x *addr, int num_conn)
{
	if(bind(addr)) {
		std::cout << "ERROR binding to provided addr" << std::endl;
		return -1;
	}
	if(Xlisten(_fd, num_conn)) {
		std::cout << "ERROR listening on provided socket" << std::endl;
		return -1;
	}
	return 0;
}

int XIAStreamSocket::bind_and_listen_on_temp_sid(int num_conn, sockaddr_x &addr)
{
	sid_key = std::make_unique<SIDKey>();
	struct addrinfo hints, *ai;
	bzero(&hints, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_XIA;
	if(Xgetaddrinfo(NULL, sid_key->to_string().c_str(), &hints, &ai)) {
		throw std::runtime_error("ERROR getting local addr");
	}
	sockaddr_x *sa = (sockaddr_x *)ai->ai_addr;
	if(bind_and_listen(sa, num_conn)) {
		throw std::runtime_error("ERROR listening on local addr");
	}
	memcpy(&addr, sa, sizeof(sockaddr_x));
	Xfreeaddrinfo(ai);
	return 0;
}

// Accept an incoming connection as an XIAStreamSocket instance
// @returns address of remote peer in 'sa' argument
// @returns accepted XIAStreamSocket
std::unique_ptr<XIAStreamSocket>
XIAStreamSocket::accept(sockaddr_x *sa)
{
	socklen_t size = sizeof(sockaddr_x);
	int newsockfd = Xaccept(_fd, (struct sockaddr *)sa, &size);
	if(newsockfd < 0) {
		throw std::runtime_error("ERROR accepting a connection");
	}
	if(size != sizeof(sockaddr_x)) {
		throw std::runtime_error("ERROR peer address is not XIA");
	}
	return std::make_unique<XIAStreamSocket>(newsockfd);
}

// Connect this stream socket to a remote address
int XIAStreamSocket::connect(const sockaddr_x *addr)
{
	return Xconnect(_fd, (const struct sockaddr *)addr, sizeof(sockaddr_x));
}

// Send size bytes or less from buf
// @returns number of bytes actually sent
int XIAStreamSocket::send(const void *buf, size_t size)
{
	return Xsend(_fd, buf, size, 0);
}

// Receives up to size bytes into buf
// @returns number of bytes received
int XIAStreamSocket::recv(void *buf, size_t size)
{
	return Xrecv(_fd, buf, size, 0);
}

// Return underlying socket file descriptor.
// TODO: Make this private to prevent use of _fd after it is closed
int XIAStreamSocket::fd()
{
	return _fd;
}
#endif // _XIASTREAMSOCKET_HH

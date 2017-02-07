/*!
  @file Xdoc.h
  @brief dummy file to contain main API documentation
*/
/*!
  @mainpage

<h1>XSocket API</h1>

This document describes the <a href=http://www.cs.cmu.edu/~xia/>eXpressive
Internet Architecture</a>  (XIA) network socket user interface.

<h2>Socket Layer Functions</h2>

For the most part, the Xsocket APIs use the same parameters as their
Berkley socket counterparts. The main difference is that XIA has introduced
a new address family (AF_INET) and the addresses passed between functions
use a new sockaddr type (sockaddr_x). This documentation describes the
APIs and the differences between XIA and Berkley sockets. The man pages
for the Berkley calls can be referred to more more detailed information
on the function calls.


<h2>XIA Function List</h2>
- Xaccept() - accept connections
- Xaccept4() - accept connections
- Xbind() - bind a socket to a DAG
- Xclose() - close the Xsocket
- Xconnect() - connect to a remote DAG
- XcreateFID() - create and register a cryptographic FID
- Xfcntl() - manipulate a socket
- Xfork() - create a child process
- Xfreeaddrinfo() - free memory returned by Xgetaddrinfo()
- Xfreeifaddrs() - free list of network addresses
- Xlisten() - listen for connections
- Xgai_strerror() - Xgetaddrinfo() error message string
- Xgetaddrinfo() - look up an address
- XgetDAGbyName() - look up an address
- Xgethostname() - get the XIA hostname
- Xgetifaddrs() - get list of network interfaces
- XgetNamebyDAG() - reverse address lookup
- Xgetpeername() - get address of connected socket
- Xgetsockname() - get address of my socket
- Xgetsockopt() - get socket options
- XmakeNewSID() - create a crypotgraphic SID & keypair
- Xnotify() - receive notification of interface changes
- Xpoll() - asyncronous I/O
- XreadLocalHostAddr() - look up the AD and HID of the local host
- Xrecv() - receive data
- Xrecvfrom() - receive datagram data
- Xrecvmsg() - receive datagram message
- XregisterName() - register our service/host name with the nameserver
- XremoveFID() - delete FID and associated keypair
- XremoveSID() - delete SID & associated keypair
- XrootDir() - get the path to the xia directory tree
- Xselect() - asyncronous I/O
- Xsend() - send data
- Xsendmsg() - send datagram message
- Xsendto() - send datagram data
- Xsetsockopt() - set socket options
- Xsocket() - create an XIA socket
- xia_ntop() - convert binary address to a string
- xia_pton() - convert a string to binary address

<h3>Content (Chunk) Oriented Functions</h3>
- add new chunk APIs here


@warning
In the current implementation, the socket returned by Xsocket is actually
a normal UDP on localhost that is used to communicate with the click
transport layer. Using this socket with normal unix socket calls will cause
unexpected behaviors.
Attempting to pass a socket created with the the normal socket function
to the Xsocket API will have similar results.

@todo add description of DAGs (who can provide?)
@todo add chunk APIs
*/


/*!
  @mainpage

  @note Doxygen is assuming we have C++ code instead of C and is doing
  some odd things. If we like this, we'll need to figure out how to 
  bend it to our will.

<h1>XSocket API</h1>

This document describes the <a href=http://www.cs.cmu.edu/~xia/>eXpressive
Internet Architecture</a>  (XIA) networking socket layer user interface. 
As currently implemented, XIA sockets leverage the standard BSD sockets, 
extendeding them to work with the XIA naming and routing schemes. 

<h2>Socket Layer Functions</h2>

These functions are used by the user process to send or receive packets 
and to do other socket operations. For more information see their 
respective descriptions.
`
Xsocket() creates a socket, Xconnect() connects a socket to a remote DAG, 
the Xbind() function binds a socket to a local DAG, listen(2) and 
Xaccept() is used to get a new socket with a new incoming connection.

Xsend() and Xsendto() send data over a socket, while Xrecv() and 
Xrecvfrom() receive data from a socket. Although there is not currently a
select method for Xsockets, the standard select() socket function can be 
used with Xsockets to check for arriving data or a readiness to send data.

Xgetsocketinfo() is similar to the traditional getsockname and 
getpeername functions returning information about the requested socket. 
Xgetsockopt() and Xsetsockopt() are used to set or get socket layer or 
protocol options. 

XputCID() and XgetCID() are used to send and receive chunks of content.

Xclose() is used to close a socket.

@note what is the purpose of Xgetsocketidlist()?

<h2>Maybe add some description of how Xsockets currently work</h2>
@note we can add links to external sites, or example code if we want.
We're not constrained to a single main documentation page.
links to other files here.

@warning 
In the current implementation, the socket used by the API is a normal UDP 
socket that is used to communicate with the click transport layer. Using 
this socket with normal unix socket calls will cause unexpected behaviors.
Attempting to pass a socket created with the the normal socket function 
to the Xsocket API will have similar results. Currently the only standard
socket function that will work correctly is select().

@note Do we want to document our Google protobuf usage, and if so, do we
want to include the class definitions for it in this document? I'm
assuming this documentation is geared toward users, not implementers 
so it should be omitted.

<h2>XIA Function List</h2>
- <a href="Xsocket_8h.html#func-members">Full API list</a>
- Xsocket() Create an XIA socket
- Xbind() Bind a socket to a DAG
- Xconnect() Connect to a remote DAG
- Xclose() Close the Xsocket
- Xsetsockopt() set socket options
- Xgetsockopt() get socket options
- Xsend() send data
- Xsendto() send datagram data
- Xrecv() receive data
- Xrecvfrom() receive datagram data
- XgetCID() get a vector of chunks of content
- XputCID() make a vector of data chunks available
- Xgetsocketinfo() get information about an Xsocket
- Xgetsocketidlist() what is this for?

<h3>Unimplemented Features</h3>
@note Do we want to include a list of standard socket library features 
that will not be implemented, or just list things that we plan to add 
at some point in the future?

- listen
- poll
- select (XIA) implementation
- non-blocking sockets
- shutdown
- read, write
- ioctl
- the flags parameter is currently ignored in all functions
*/


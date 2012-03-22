============================
Functions
============================

.. function:: Xaccept(sockfd)
	
	Accept an incoming connection on *sockfd*. Return the new Xsocket id on success, -1 on error.
	
	.. note:: Unlike standard sockets, there is currently no Xlisten function. Callers must create the listening socet by calling Xsocket with the XSOCK_STREAM transport_type and bind it to a source DAG with Xbind. XAccept may then be called to wait for connections.

.. function:: Xbind(sockfd, sDAG)

	Bind the local socket, *sockfd*, to *sDAG*. *sDAG* should be a SID. Return 0 on success, -1 on error.

.. function:: Xclose(sockfd)

	Causes click to tear down the underlying XIA and also closes the UDP socket used to talk to click. Return 0 on success, -1 on error.

.. function:: Xconnect(sockfd, dest_DAG)

	Connect to a remote XIA SID socket over control socket *sockfd*. This is only valid on sockets created with the XSOCK_STREAM transport type. *dest_DAG*'s primary intent should be an SID. Return 0 on success, -1 on error.

.. function:: XgetCID(sockfd, cDAG, dlen)

	Request a chunk of content with address *cDAG* of length *dlen* over control socket *sockfd*. *sockfd* must be of type XSOCK_CHUNK. The primary intent of *cDAG* must be a CID. *dlen* is currently unused. Return 0 on success, -1 on error.

.. function:: XgetCIDList(sockfd, cDAGv, numCIDs)

	Request a list of *numCIDs* chunks (*cDAGv*) over control socket *sockfd*. *sockfd* must be of type XSOCK_CHUNK. Return 0 on success, -1 on error.

.. function:: XgetCIDListStatus(sockfd, cDAGv, numCIDs)

	Check the status of the requested chunks in *cDAGv*. On return the status of each chunk in *cDAGv* will be set. *sockfd* must be of type XSOCK_CHUNK. Return 1 if all chunks in *cDAGv* are ready to be read, 0 if one or more chunks are in waiting state, or -1 if an invalid CID was specified or a socket error occurred.

.. function:: XgetCIDStatus(sockfd, cDAG, dlen)

	Return 1 if *cDAG* is ready to be read, 0 if we are waiting to receive the chunk, or -1 on error. *sockfd* must be of type XSOCK_CHUNK.

.. function:: XputCID(sockfd, data, flags, cDAG, dlen)

	Publish *data* as a content chunk over control socket *sockfd* (must be of type XSOCK_CHUNK). *cDAG* is the chunk's new address (and has length *dlen*). *flags* not currently used. Return 0 on success, -1 on error.

.. function:: XreadCID(sockfd, len, flags, cDAG, dlen)

	Read a content chunk with address *cDAG* (of length *dlen*, currently unused) over control socket *sockfd* (must be of type XSOCK_CHUNK). *len* is the maximum size of the content. *flags* currently unused. Return the chunk.

.. function:: Xrecv(sockfd, len, flags)

	Read at most *len* bytes from *sockfd*. *flags* not currently used. Return the received data.

	.. note:: In cases where more data is received than specified by the caller, the excess data will be stored in the socket state structure and will be returned from there rather than from Click. Once the socket state is drained, requests will be sent through to Click again.

.. function:: Xrecvfrom(sockfd, len, flags)

	Read at most *len* bytes from *sockfd*. *flags* not currently used. Returns a tuple *(data, sDAG)* where *data* is the received data and *sDAG* is the sender's address.

	.. note:: In cases where more data is received than specified by the caller, the excess data will be stored in the socket state structure and will be returned from there rather than from Click. Once the socket state is drained, requests will be sent through to Click again.

.. function:: Xsend(sockfd, data, len, flags)

	Send *data* over *sockfd*, where *len* is the length of the data (currently Xsend is limited to sending at most XIA_MAXBUF bytes). *flags* not currently used. Return the number of bytes sent on success or -1 on error.

.. function:: Xsendto(sockfd, data, len, flags, dDAG, dlen)

	Send *data* to *dDAG* over *sockfd*, where *len* is the length of the data (currently limited to at most XIA_MAXBUF bytes) and *dlen* is the length of the address *dDAG*. *flags* currently unused. Return the number of bytes sent on success or -1 on error.

.. function:: Xsocket(transport_type)

	Create an XIA socket. Must be the first Xsocket function called. *transport_type* must be one of XSOCK_STREAM (for reliable communication to an SID), XSOCK_DGRAM (for a lighter weight connection to an SID, but without guaranteed delivery), XSOCK_CHUNK (for getting/putting content chunks), or XSOCK_RAW (for a raw socket allowing direct edits to the header).

	Return socket ID on success, -1 on error.

	When called it creates a socket to connect to Click using UDP packets using a random local port. It then sends an open request to Click, from that socket. It waits (blocking) for a reply from Click. The control info is encoded in the google protobuf message (encapsulated within UDP message).

	.. warning:: In the current implementation, the returned socket is a normal UDP socket that is used to communicate with the click transport layer. Using this socket with normal unix socket calls will cause unexpected behaviors. Attempting to pass a socket created with the the normal socket function to the Xsocket API will have similar results.

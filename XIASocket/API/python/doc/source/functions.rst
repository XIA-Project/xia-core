============================
Functions
============================

.. function:: Xaccept(sockfd)
	
	The :func:`Xaccept` system call is is only valid with Xsockets created with the XSOCK_STREAM transport type. It accepts the first available connection request for the listening socket, *sockfd*, creates a new connected socket, and returns a new Xsocket descriptor referring to that socket. The newly created socket is not in the listening state. The original socket *sockfd* is unaffected by this call.

	:func:`Xaccept` does not currently have a non-blocking mode, and will block until a connection is made. However, the standard socket API calls select and poll may be used with the Xsocket. Either function will deliver a readable event when a new connection is attempted and you may then call :func:`Xaccept` to get a socket for that connection.

	.. note:: Unlike standard sockets, there is currently no Xlisten function. Callers must create the listening socet by calling Xsocket with the XSOCK_STREAM transport_type and bind it to a source DAG with :func:`Xbind`. :func:`Xaccept` may then be called to wait for connections.
	
	Return a non-negative integer that is the new Xsocket id on success, -1 on error

.. function:: XallocCacheSlice(policy, ttl, size)

	Init a context to store meta data, e.g. cache *policy*, *ttl*, and *size*. Serve as an application handler when putting content. This can replace the old socket() call, because we don't really need a socket, but an identifier to the application.

	The identifier uses getpid(), but can be replaced by any unique ID.

	Return an ChunkContext object.

.. function:: Xbind(sockfd, sDAG)

	Assign the DAG *sDAG* to to the Xsocket referred to by *sockfd*. The *sDAG*'s final intent should be a valid SID.

	It is necessary to assign a local DAG using :func:`Xbind()` before an XSOCK_STREAM socket may receive connections (see :func:`Xaccept`).

	An un-bound Xsocket will be given a random local SID that is currently not available to the application.

	Return 0 on success, -1 on error

.. function:: Xclose(sockfd)

	Causes the XIA transport to tear down the underlying XIA socket state and also closes the UDP control socket *sockfd* used to talk to the transport.

	Return 0 on success, -1 on error

.. function:: Xconnect(sockfd, dDAG)

	The :func:`Xconnect` call connects the socket referred to by *sockfd* to the SID specified by *dDAG*. It is only valid for use with sockets created with the XSOCK_STREAM Xsocket type.

	.. note:: :func:`Xconnect` differs from the standard connect API in that it does not currently support use with Xsockets created with the XSOCK_DGRAM socket type.

	Return 0 on success, -1 on error

.. function:: XfreeCacheSlice(context)

	TODO: finish

.. function:: XfreeChunkInfo(info_list)

	TODO: finish

.. function:: XgetChunkStatus(sockfd, dag)

	TODO: finish

.. function:: XgetChunkStatuses(sockfd, status_list, num_cids)

	TODO: finish

.. function:: XgetDAGbyName(name)

	TODO: finish

.. function:: Xgetsockopt(sockfd, optname)

	Retrieve the settings of the underlying Xsocket in the Click layer. It does not access the settings of *sockfd* itself, which is the control socket used by the API to communicate with Click.

	Supported Options:
	XOPT_HLIM Retrieves the 'hop limit' element of the XIA header as an integer value
	XOPT_NEXT_PROTO Gets the next proto field in the XIA header

	Return the value associated with *optname*.

.. function:: XputBuffer(context, data, chunk_size)

	TODO: finish

.. function:: XputChunk(context, data)

	TODO: finish

.. function:: XputFile(context, file_name, chunk_size)

	TODO: finish

.. function:: XreadChunk(sockfd, length, flags, content_dag)

	TODO: finish
	Read a content chunk with address *cDAG* over control socket *sockfd* (must be of type XSOCK_CHUNK). *flags* currently unused. Return the chunk.

.. function:: Xrecv(sockfd, length, flags)

	Read at most *length* bytes from *sockfd*. *flags* not currently used. Return the received data.

	.. note:: In cases where more data is received than specified by the caller, the excess data will be stored in the socket state structure and will be returned from there rather than from Click. Once the socket state is drained, requests will be sent through to Click again.

.. function:: XreadLocalHostAddr(sockfd)

	TODO: finish

.. function:: XreadNameServerDAG(sockfd)

	TODO: finish

.. function:: Xrecv(sockfd, length, flags)

	Retrieve at most *length* bytes of data from *sockfd*, which must be of type XSOCK_STREAM and have previously been connected via :func:`Xaccept` or :func:`Xconnect`. *flags* not currently used.

	:func:`Xrecv` does not currently have a non-blocking mode, and will block until a data is available on *sockfd*. However, the standard socket API calls select and poll may be used with the Xsocket. Either function will deliver a readable event when a new connection is attempted and you may then call :func:`Xrecv` to get the data.

	.. note:: In cases where more data is received than specified by *length*, the excess data will be stored at the API level. Subsequent :func:`Xrecv` calls return the stored data until it is drained, and will then resume requesting data from the transport.

	Return the data received.

.. function:: Xrecvfrom(sockfd, length, flags)

	Retrieves at most *length* bytes of data from *sockfd*, which must be of type XSOCK_DGRAM. Unlike the standard recvfrom API, it will not work with sockets of type XSOCK_STREAM.

	:func:`XrecvFrom` does not currently have a non-blocking mode, and will block until a data is available on sockfd. However, the standard socket API calls select and poll may be used with the Xsocket. Either function will deliver a readable event when a new connection is attempted and you may then call :func:`XrecvFrom` to get the data.

	.. note:: In cases where more data is received than specified by *length*, the excess data will be stored in the socket state structure and will be returned from there rather than from Click. Once the socket state is drained, requests will be sent through to Click again.

	Return a tuple containing the data received and the sender's dag. Example use:

	.. code-block:: python

		sock = Xsocket(XSOCK_DGRAM)
		(data, sender_dag) = Xrecvfrom(sock, XIA_MAXBUF, 0)

.. function:: XregisterName(name, DAG)

	TODO: finish

.. function:: XremoveChunk(context, cid)

	TODO: finish

.. function:: XrequestChunk(sockfd, dag)

	TODO: finish

.. function:: XrequestChunks(sockfd, chunk_list, num_chunks)

	TODO: finish

.. function:: Xsend(sockfd, data, flags)

	Send *data* over *sockfd* (currently :func:`Xsend` is limited to sending at most XIA_MAXBUF bytes). *flags* not currently used. The :func:`Xsend` call may be used only when the socket is in a connected state (so that the intended recipient is known). It only works with an Xsocket of type XSOCK_STREAM that has previously been connecteted with :func:`Xaccept` or :func:`Xconnect`.

	Return the number of bytes sent on success, -1 on error.

.. function:: Xsendto(sockfd, data, len, flags, dDAG, dlen)

	Send a datagram containing *data* of length *len* bytes to *dDAG* (where the length of *dDAG* is *dlen*). *flags* currently unused. The length of *data* is currently limited to XIA_MAXBUF bytes. 

	.. note:: Unlike a standard socket, :func:`Xsendto` is only valid on Xsockets of type XSOCK_DGRAM.

	Return number of bytes sent on success, -1 on error.

.. function:: Xsetsockopt(sockfd, optname, optval, optlen)

	Set the option *optname* on the underlying Xsocket in the Click layer to *optval* (of length *optlen* bytes). It does not affect *sockfd* itself, which is the control socket used by the API to communicate with Click.

	Supported Options:
	XOPT_HLIM Sets the 'hop limit' (hlim) element of the XIA header to the specified integer value. (Default is 250)
	XOPT_NEXT_PROTO Sets the next proto field in the XIA header

	Return 0 on success, -1 on error.

.. function:: Xsocket(transport_type)

	Create an XIA socket. Must be the first Xsocket function called. *transport_type* must be one of XSOCK_STREAM (for reliable communication to an SID), XSOCK_DGRAM (for a lighter weight connection to an SID, but without guaranteed delivery), XSOCK_CHUNK (for getting/putting content chunks), or XSOCK_RAW (for a raw socket allowing direct edits to the header).

	Return socket ID on success, -1 on error.

	.. warning:: In the current implementation, the returned socket is a normal UDP socket that is used to communicate with the click transport layer. Using this socket with normal unix socket calls will cause unexpected behaviors. Attempting to pass a socket created with the the normal socket function to the Xsocket API will have similar results.

.. function:: XupdateAD(sockfd, newad)

	TODO: finish

.. function:: XupdateNameServerDAG(sockfd, nsDAG)

	TODO: finish

============================
Functions
============================

The python Xsocket API consists of the following functions, which to a large degree mimic their C counterparts:

**General Xsocket Functions**

* :func:`Xsocket` create an XIA socket
* :func:`Xbind` bind a socket to a DAG
* :func:`Xclose` close the Xsocket
* :func:`Xsetsockopt` set socket options
* :func:`Xgetsockopt` get socket options
* :func:`XgetDAGbyName` convert a name to a DAG that can be used by other Xsocket functions
* :func:`XreadLocalHostAddr` look up the AD, HID, and 4ID of the local host
* :func:`XregisterName` register our service/host name with the nameserver

**Stream Oriented Functions**

* :func:`Xaccept` wait for stream connections
* :func:`Xconnect` connect to a remote DAG
* :func:`Xsend` send data
* :func:`Xrecv` receive data

**Datagram Oriented Functions**

* :func:`Xsendto` send datagram data
* :func:`Xrecvfrom` receive datagram data

**Content (Chunk) Oriented Functions**

* :func:`XallocCacheSlice` allocate space in the local content cache
* :func:`XfreeCacheSlice` release the reserved local cache space
* :func:`XputChunk` make a single chunk of content available
* :func:`XputFile` make a file available as one or more chunks
* :func:`XputBuffer` make a block of memory available as one or more chunks
* :func:`XrequestChunk`, :func:`XrequestChunks` bring one or more chunks of content form the network to the local machine
* :func:`XgetChunkStatus`, :func:`XgetChunkStatuses` get the readiness status of one or more chunks of content
* :func:`XreadChunk` load a single chunk into memory

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

    This function closes the socket used to communicate with the click and frees the allocated ChunkContext *context*.

    Returns 0 on success, -1 on error.

    .. note:: This does not tear down the content cache itself. It will live until the content in it expires. To clear the cache in the current release, :func:`XremoveChunk` can be called for each chunk of data.

.. function:: XgetChunkStatus(sockfd, dag)

    Return an integer indicating if the content chunk specified by *dag* is available to be read. It is a simple wrapper around the :func:`XgetChunkStatuses` function which does the actual work. *sockfd* must be of type XSOCK_CHUNK.

    Returns:
        READY_TO_READ if the requested chunk is ready to be read. 
        WAITING_FOR_CHUNK if the requested chunk is still in transit. 
        REQUEST_FAILED if the specified chunk has not been requested, or if a socket error occurs. 
        INVALID_HASH if the CID hash does not match the content payload. 

.. function:: XgetChunkStatuses(sockfd, status_list, num_cids)

	TODO: finish

.. function:: XgetDAGbyName(name)

    Return the DAG registered to *name*. *name* should be a string such as www_s.example.xia or host.example.xia. By convention services are indicated by '_s' appended to the service name. 

.. function:: Xgetsockopt(sockfd, optname)

	Retrieve the settings of the underlying Xsocket in the Click layer. It does not access the settings of *sockfd* itself, which is the control socket used by the API to communicate with Click.

	Supported Options:
	XOPT_HLIM Retrieves the 'hop limit' element of the XIA header as an integer value
	XOPT_NEXT_PROTO Gets the next proto field in the XIA header

	Return the value associated with *optname*.

.. function:: XputBuffer(context, data, chunk_size)

    Publish *data* as chunks of content of maximum size *chunk_size* in the cache slice corresponding to *context*. *chunk_size* must not be larger than XIA_MAXCHUNK. If *chunk_size* is 0, the default chunk size is used.

    :func:`XputBuffer` calls :func:`XputChunk` internally and has the same requiremts as that function.

    On success, the CIDs of the returned ChunkInfo objects are set to the 40 character hashes of the published chunks. Each CID is not a full DAG, and must be converted to a DAG before the client applicatation can request it, otherwise an error will occur.

    If the file causes the cache slice to grow too large, the oldest content chunk(s) will be removed to make enough space for the new chunk(s).

    Return a tuple of ChunkInfo objects describing the chunks that were published.

.. function:: XputChunk(context, data)

    Make *data* available on the network as a single chunk of content. The size of *data* must be less than XIA_MAXCHUNK On success, the CID of the returned ChunkInfo object is set to the 40 character hash of the content data. The CID is not a full DAG, and must be converted to a DAG before the client applicatation can request it, otherwise an error will occur.

    If the chunk causes the cache slice to grow too large, the oldest content chunk(s) will be reoved to make enough space for this chunk.

    Return a ChunkInfo object describing the published chunk.

.. function:: XputFile(context, file_name, chunk_size)

    Publish the file *file_name* as a series of content chunks of maximum size *chunk_size*. *chunk_size* must not be larger than XIA_MAXBUF. If *chunk_size* is 0, the default chunk size is used.

    :func:`XputFile` calls :func:`XputChunk` internally and has the same requiremts as that function.

    On success, the CIDs of the returned ChunkInfo objects are set to the 40 character hashes of the published chunks. Each CID is not a full DAG, and must be converted to a DAG before the client applicatation can request it, otherwise an error will occur.

    If the file causes the cache slice to grow too large, the oldest content chunk(s) will be reoved to make enough space for the new chunk(s).
    
    Return a tuple of ChunkInfo objects describing the chunks that were published.

.. function:: XreadChunk(sockfd, length, flags, content_dag)

    Read at most *length* bytes of the content chunk with address *content_dag* over *sockfd*, which must be of type XSOCK_CHUNK.

    Note that *content_dag* must be a full DAG, not just the 40 byte hash returned by XputChunk. For instance: "RE ( AD:AD0 HID:HID0 ) CID:<hash>" where <hash> is the 40 character hash of the content chunk generated by the sender. The :func:`XputChunk` API call only returns <hash>. Either the client or server application must generate the full DAG that is passed to this API call.

    Return the data read.

.. function:: Xrecv(sockfd, length, flags)

	Read at most *length* bytes from *sockfd*. *flags* not currently used. Return the received data.

	.. note:: In cases where more data is received than specified by the caller, the excess data will be stored in the socket state structure and will be returned from there rather than from Click. Once the socket state is drained, requests will be sent through to Click again.

.. function:: XreadLocalHostAddr(sockfd)

    Return a tuple containing the HID, AD, and 4ID assigned to the host by the XIA stack. Among other things, this allows the application to share its address with other applications.

    Example use:
	.. code-block:: python

		sock = Xsocket(XSOCK_STREAM) # socket may be of any type; XSOCK_STREAM used here as an example
		(myAD, myHID, my4ID) = XreadLocalHostAddr(sock)

.. function:: XreadNameServerDAG(sockfd)

	Return the DAG of the local nameserver, configured by XHCP.

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

    Register a host or service name with the XIA nameserver. By convention services are indicated by '_s' appended to the service name. 

    This is a very simple implementation and will be replaced in a future release. This version does not check correctness of the name or dag, nor does it check to ensure that the client is allowed to bind to *name*.

    Return 0 on success, -1 on error.

.. function:: XremoveChunk(context, cid)

    Remove the content with ID *cid* from the content cache. A successful return code will be returned regardless of whether or not the chunk was already expired out of the cache. *cid* must be the value returned from one of the Xput... functions; a full DAG will not be recognized as a valid identifier.

    Return 0 on success, -1 on error.

.. function:: XrequestChunk(sockfd, dag)

    Load a content chunk with address *dag* into the XIA content cache. *sockfd* must by of type XSOCK_CHUNK. :func:`XrequestChunk` does not return the requested data, it only causes the chunk to be loaded into the local content cache. :func:`XgetChunkStatus` may be called to get the status of the chunk to determine when it becomes available. Once the chunk is ready to be read, :func:`XreadChunk` should be called get the actual content chunk data.

    :func:`XrequestChunk` is a simple wrapper around the :func:`XrequestChunks` API call.

    Return 0 on success, -1 on error.

.. function:: XrequestChunks(sockfd, chunk_list, num_chunks)

    Load a list of *num_chunks* content chunks, *chunk_list* into the XIA content cache. It does not return the requested data, it only causes the chunk to be loaded into the local content cache. :func:`XgetChunkStatuses` may be called to get the status of the chunk to determine when it becomes available. Once the chunk is ready to be read, :func:`XreadChunk` should be called get the actual content chunk data.

    :func:`XrequestChunk` can be used when only a single chunk is requested.

    Return 0 on success, -1 on error.

.. function:: Xsend(sockfd, data, flags)

	Send *data* over *sockfd* (currently :func:`Xsend` is limited to sending at most XIA_MAXBUF bytes). *flags* not currently used. The :func:`Xsend` call may be used only when the socket is in a connected state (so that the intended recipient is known). It only works with an Xsocket of type XSOCK_STREAM that has previously been connecteted with :func:`Xaccept` or :func:`Xconnect`.

	Return the number of bytes sent on success, -1 on error.

.. function:: Xsendto(sockfd, data, flags, dDAG)

	Send a datagram containing *data* to *dDAG*. *flags* currently unused. The length of *data* is currently limited to XIA_MAXBUF bytes. 

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


.. function:: XupdateNameServerDAG(sockfd, nsDAG)


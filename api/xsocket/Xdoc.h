/*!
  @file Xdoc.h
  @brief dummy file to contain main API documentation
*/  
/*!
  @mainpage

<h1>XSocket API</h1>

This document describes the <a href=http://www.cs.cmu.edu/~xia/>eXpressive
Internet Architecture</a>  (XIA) networking socket layer user interface. 
As currently implemented, XIA sockets leverage the standard BSD sockets, 
extendeding them to work with the XIA naming and routing schemes. 

<h2>Socket Layer Functions</h2>

These functions are used by the user process to send or receive packets 
and to do other socket operations. For more information see their 
respective descriptions.

Xsocket() creates a socket, Xconnect() connects a socket to a remote DAG, 
the Xbind() function binds a socket to a local DAG,
Xaccept() is used to get a new socket with a new incoming connection.

Xsend() and Xsendto() send data over a socket, while Xrecv() and 
Xrecvfrom() receive data from a socket. Although there is not currently a
select  or poll method for Xsockets, the standard select() and poll()
socket function can be used with Xsockets to check for arriving data or
a readiness to send data.

Xgetsockopt() and Xsetsockopt() are used to set or get socket layer or 
protocol options. 

XallocCacheSlice() reserves a block of memory in the local machine's 
content cache for published chunks of content. XfreeCacheSlice() disconnects
from the reserved slice and potentially releases the content depending on
how the slice was allocated.

XputChunk() places a single chunk of content into a cache slicei so that it
is available on the network. XputFile() and XputBuffer() place one or more
chunks of content into the slice. XfreeChunkInfo() releases the status
array allocated by the XputFile() and XputBuffer() functions.

XrequestChunk() and XrequestChunks() bring one or more chunks of content from
the network to the local machine. XgetChunkStatus() and XgetChunkStatuses()
check to see if the requested content is available to be read. XreadChunk()
is then used to get the content into the application. 

Xclose() is used to close a socket.

@warning 
In the current implementation, the socket used by the API is a normal UDP 
socket that is used to communicate with the click transport layer. Using 
this socket with normal unix socket calls will cause unexpected behaviors.
Attempting to pass a socket created with the the normal socket function 
to the Xsocket API will have similar results. Currently the only standard
socket function that will work correctly is select().

<h2>XIA Function List</h2>
- <a href="Xsocket_8h.html#func-members">Full API list</a>
<h3>General Xsocket Functions</h3>
- Xsocket() create an XIA socket
- Xbind() bind a socket to a DAG
- Xclose() close the Xsocket
- Xsetsockopt() set socket options
- Xgetsockopt() get socket options
- XgetDAGbyName() convert a name to DAG that can be used by other Xsocket functions
- XreadLocalHostAddr() look up the AD and HID of the local host
- XregisterName() register our service/host name with the nameserver
<h3>Stream Oriented Functions</h3>
- Xaccept() wait for stream connections
- Xconnect() connect to a remote DAG
- Xsend() send data
- Xrecv() receive data
<h3>Datagram Oriented Functions</h3>
- Xsendto() send datagram data
- Xrecvfrom() receive datagram data
<h3>Content (Chunk) Oriented Functions</h3>
- XallocCacheSlice() allocate a space in the local content cache
- XfreeCacheSlice() release the reserved local cache space
- XputChunk() make a single chunk of content available
- XputFile() make a file available as one or more chunks
- XputBuffer() make a block of memory available as one or more chunks
- XfreeChunkInfo() frees the chunk status array allocated by XputFile() and XputBuffer()
- XrequestChunk(), XrequestChunks() bring one or more chunks of content from
the network to the local machine
- XgetChunkStatus(), XgetChunkStatuses() get the rediness status of one or more
chunks of content
- XreadChunk() load a single chunk into memory


@todo add description of DAGs (who can provide?)
@todo add description of how to build an xsocket app including description
of the xsocketini.conf file 
@todo add description of chunking and how it works
@todo what's missing, what needs to be cleaned up more?
*/


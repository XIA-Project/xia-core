from c_xsocket import *
    
# Set up connection with click via Xsocket API
set_conf("xsockconf_python.ini", "malicious_router_publisher.py")
sock = Xsocket(XSOCK_CHUNK)  # socket for reading local addr
print XreadLocalHostAddr(sock)
    
# Allocate a local cache slice
chunk_context = XallocCacheSlice(POLICY_DEFAULT, 0, 0)

# Publish the picture
cid = XputFile(chunk_context, 'anon.jpg', XIA_MAXBUF)
XputFile(chunk_context, 'www/img/image.jpg', XIA_MAXBUF)  # Publish so that malicious router has a route to this CID

# Print the CID
print 'Published chunk with CID %s' % cid[0].cid

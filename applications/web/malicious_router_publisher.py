import sys
import os

# find the path to xia-core
XIADIR=os.getcwd()
while os.path.split(XIADIR)[1] != 'xia-core':
    XIADIR=os.path.split(XIADIR)[0]
sys.path.append(XIADIR + '/api/lib')

from c_xsocket import *

sock = Xsocket(XSOCK_CHUNK)  # socket for reading local addr

print XreadLocalHostAddr(sock)

# Allocate a local cache slice
chunk_context = XallocCacheSlice(POLICY_DEFAULT, 0, 0)

# Publish the picture
anon_cid = XputFile(chunk_context, 'anon.jpg', XIA_MAXBUF)
html_cid = XputFile(chunk_context, 'www/simple_malicious_explanation_processed.html', XIA_MAXBUF)
#XputFile(chunk_context, 'www/img/image.jpg', XIA_MAXBUF)  # Publish so that malicious router has a route to this CID
dongsu_cids = XputFile(chunk_context, 'photo-2.jpg', 1000)

print 'dongsu image:'
for cid in dongsu_cids:
    print cid.cid

anon_cids = XputFile(chunk_context, 'anon.jpg', 1000)

print 'anon image:'
for cid in anon_cids:
    print cid.cid

# Print the CID
print 'Published image chunk with CID %s' % anon_cid[0].cid
print 'Published html chunk with CID %s' % html_cid[0].cid

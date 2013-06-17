This code has been modified to work with the XIA interposition wrapper. 

See the interposition library [wiki page](https://github.com/XIA-Project/xia-core/wiki/XIA-Interposition-Library/) for more information.

* httpd can run as a normal sockets applicaton or an XIA app via use of the interposition library.
*simpleclient will only run as an xia app due to how it was coded

To run under XIA, use the following commands:
* xia-core/bin/xwrap ./httpd -x
* xia-core/bin/xwrap ./simpleclient

Caveats:
* simpleclient is a quick hack and doesn't look for end of file, it currently needs to be killed manually.

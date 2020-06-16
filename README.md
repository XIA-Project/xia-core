XIA-core
=======

We are proud to announce version 1.0 of XIA (eXpressive Internet Architecture).
XIA (eXpressive Internet Architecture) is a joint research project between
Carnegie Mellon University, Boston University, and the University of Wisconsin
at Madison. 

Information and papers on XIA are available on the [XIA project page](http://cs.cmu.edu/~xia)
XIA design information can be found on the [XIA Wiki](https://github.com/XIA-Project/xia-core/wiki). 
Information on building and using XIA is also available on the wiki.

The main git branch will always contain the most recently released version of
XIA. Other branches will contain ongoing development or research projects and 
are not guaranteed to function correctly. If you are interested in using one
of the other branches, contact us at the address above for status of the branch.

Contact
-------
- Support and general XIA usage questions should be sent to the support email address <xia-users-help@cs.cmu.edu>. 
This address is monitored by several of the XIA developers and we strive to respond in a timely fashion.
- [XIA-Users](https://mailman.srv.cs.cmu.edu/mailman/listinfo/xia-users) is an unmoderated general discussion mailing 
list for XIA. Everyone is welcome to join and participate in the discussion, whether you are an XIA user or developer, 
or are just curious about to know more about XIA.  This is the place to post ideas, share problems and solutions, 
and discuss the design and goals of XIA. 
- [XIA-Announce](https://mailman.srv.cs.cmu.edu/mailman/listinfo/xia-announce) is a read-only list for announcements 
of new XIA releases and events. This list is open to everyone.


Related Projects
----------------
- [Native Linux kernel implementation of XIA](https://github.com/AltraMayor/XIA-for-Linux)
- [XIA Wireshark plugin](https://github.com/AltraMayor/XIA-for-Linux/wiki/Debugging-the-Linux-kernel#Wireshark_with_XIA_support)

Release 2.0
-----------
* Intrinsic Security (add information)
* The xwrap interposition now converts unmodified sockets applications to run over XIA (experimental)
* API changes
  - Non-blocking I/O is now supported
  - The Xlisten call is required in server applications

Release 1.1.1
----------------
* Bug fixes
* Merged the push branch containing experimental code for push semantics for CIDs.

Release 1.1
----------------
* Added scripts to run large scape 4ID experiments over PlanetLab.
* Updated GENI scripts for use with Ubuntu 12.04 and above.
* The xianet script has been updated to support automatically booting arbitrary
network configurations based on the click configuration files.
* Automatically generate the xsockconf.ini config files at runtime when running
the local topology.
* Added xnetcat utility, see the standard netcat man page for usage information.
* Added Xssl API for use in creating encrypted connections.
* Added a an interposition library that can be used to ease porting of standard
Berkeley socket applications.
* Implemented partial support Mac OS X. Local topologies works, single host
configurations are not supported yet. Note: OS X support currently only works
with Xcode 4.x. An update to the Click code will be required for use with Xcode
5.x.
* Added new example applications.
* The XIA daemons now log to syslog rather than stdout.
* More compatability with Berleley sockets added to the Xsocket API.
* Fixed routing issues that could occur between hosts in the same AD.
* Many bug fixes.
 
Release 1.0
----------------
* Added support for 4IDs which gives XIA the ability to be encapsulated in IP
for traversing non-XIA aware networks.
* Modified [Xsocket API](http://cs.cmu.edu/~xia/api/c) to more closely match
the standard socket API.
* Added click explorer utility to make it easy to monitor and control the
click XIA elements.
* The XIA directory tree has been reorganized to group related components
together (applications, experiments, apis, etc) rather than scatter them
throughout the tree.
* Makefiles are improved so that XIA can be built statically or dynamically or
with debug flags with a single make command instead of requiring hand edits to
different files.
* Upgraded to click version 2.0.1.
* Added capability to build statically linked version of XIA for use on networks
such as PlanetLab.
* Setting up an XIA network is more automated, for example, most nodes can be
brought up with a single command.
* Communication between the application level and Click has been changed so that
root access is no longer requried to run XIA. This allows XIA to be used on 
PlanetLab nodes where root access is not available.
* XIA now works on the latest releases of Fedora and Ubuntu.
* Various bug fixes.


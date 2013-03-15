XIA-core
=======

We are proud to announce version 1.0 of XIA (eXpressive Internet Architecture). XIA (eXpressive Internet Architecture) is a joint research project between Carnegie Mellon University, Boston University, and the University of Wisconsin at Madison. 

Information and papers on XIA are available on the [XIA project page](http://cs.cmu.edu/~xia)
XIA design information can be found on the [XIA Wiki](https://github.com/XIA-Project/xia-core/wiki). Information on building and using XIA is also available on the wiki.

Help on using XIA is available at <xia-users-help@cs.cmu.edu>

The main git branch will always contain the most recently released version of XIA. Other branches will contain ongoing development or research projects and are not guaranteed to function correctly. If you are interested in using one of the other branches, contact us at the address above for status of the branch.

Related Projects
----------------
- [Native Linux kernel implementation of XIA](https://github.com/AltraMayor/XIA-for-Linux)
- XIA Wireshark plugin
  * [Source](https://github.com/cjdoucette/wireshark-xia)
  * [32 bit debian packages](https://github.com/cjdoucette/wireshark-xia-pkg-i386)
  * [64 bit debian packages](https://github.com/cjdoucette/wireshark-xia-pkg)


Release 1.0
----------------
* Added support for 4IDs which gives XIA the ability to be encapsulated in IP for traversing non-XIA aware networks.
* Modified [Xsocket API](http://cs.cmu.edu/~xia/api/c) to more closely match the standard socket API.
* Added click explorer utility to make it easy to monitor and control the click XIA elements.
* The XIA directory tree has been reorganized to group related components together (applications, experiments, apis, etc) rather than scatter them throughout the tree.
* Makefiles are improved so that XIA can be built statically or dynamically or with debug flags with a single make command instead of requiring hand edits to different files.
* Upgraded to click version 2.0.1.
* Added capability to build statically linked version of XIA for use on networks such as PlanetLab.
* Setting up an XIA network is more automated, for example, most nodes can be brought up with a single command.
* Communication between the application level and Click has been changed so that root access is no longer requried to run XIA. This allows XIA to be used on PlanetLab nodes where root access is not available.
* XIA now works on the latest releases of Fedora and Ubuntu.
* Various bug fixes.

Related Projects
----------------
- [XIA Wireshark plugin](https://github.com/cjdoucette/wireshark-xia)
- [Native Linux implementation of XIA](https://github.com/AltraMayor/XIA-for-Linux)

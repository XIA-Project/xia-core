xcache: XIA External cache
==========================
This folder contains the implementation of XIA external cache. Click
communicates with xcache over UDP connection on port 1444. Xcache
maintains cache for all 'virtual' network devices running under
click. Source code has been organized into 3 layered structure:

- **xcache-click interface**
  (xcache.c)
  Main entry/exit layer. Handles packets to/from click.

- **xcache controller**
  (xcache_controller.c)
  This layer manages multiple click devices. Whenever a request comes
  from click, this layer identifies corresponding 'cache slice' and
  forwards the request towards it. A Network device is always
  identified by HID.

- **xcache slices**
  (xcache_slices.c)
  All the per client policies/timeouts are handled here.

- **tests**
  Folder tests contain some basic test programs that can be used to
  verify the functionality of xcache.

- **plugins**
  Xcache can be extended by adding plugins to it. This folder contains
  plugins that can added to xcache.

TODOs
-----
- Implement LRU
- DENIED packet.
- What to do if STORE request is received for cache object which is
  already present in the cache?

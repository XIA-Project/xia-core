#ifndef __XCACHE_CONTROLLER_H__
#define __XCACHE_CONTROLLER_H__
#include <map>
#include <iostream>
#include "xcache_cmd.pb.h"
#include "logger.h"
#include "slice.h"
#include "meta.h"
#include "store_manager.h"

/**
 * XcacheController:
 * @brief Abstract class that defines a eviction policy
 * Xcache eviction policy decides which content object to evict. The policy module
 * does not need to care about actually storing the data. It only deals in terms of
 * "meta"data.
 */

class XcacheController {
private:
  std::map<std::string, XcacheMeta *> metaMap;
  std::map<int32_t, XcacheSlice *> sliceMap;

  int newSlice(XcacheCommand *, XcacheCommand *);
  int startXcache(void);
  XcacheSlice *lookupSlice(XcacheCommand *);
  XcacheStoreManager storeManager;

public:
  XcacheController() {
    std::cout << "Reached Constructor\n";
  }

  void addMeta(std::string str, XcacheMeta *meta) {
    std::cout << "Reached AddMeta\n";
    metaMap[str] = meta;
  }

  void searchMeta(std::string str) {
    std::map<std::string, XcacheMeta *>::iterator i = metaMap.find(str);

    std::cout << "Reached SearchMeta\n";
    if(i != metaMap.end()) {
      i->second->print();
    } else {
      std::cout << "Not Found\n";
    }
  }

  void handleCli(void);
  void handleUdp(int);
  int handleCmd(XcacheCommand *, XcacheCommand *);
  void status(void);
  void run(void);

  int search(XcacheCommand *, XcacheCommand *);
  int store(XcacheCommand *);
  int storeFinish(XcacheMeta *, std::string);
  void remove(void);

};
#endif

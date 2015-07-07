#ifndef __XCACHE_SLICE_H__
#define __XCACHE_SLICE_H__

#include <iostream>
#include <map>
#include "../click-2.0.1/include/clicknet/xia.h"
#include "policy.h"
#include <stdint.h>

class XcacheMeta;
class XcachePolicy;

class XcacheSlice {
private:
  std::map<std::string, XcacheMeta *> metaMap;
  uint64_t maxSize, currentSize;
  int32_t contextID;

  uint64_t ttl;
  XcachePolicy policy;

public:
  XcacheSlice(int32_t contextID);

  void setTtl(uint32_t ttl) {
    this->ttl = ttl;
  }

  void setSize(uint64_t size) {
    this->maxSize = size;
  }


  void setPolicy(XcachePolicy);

  void addMeta(XcacheMeta *);

  int store(XcacheMeta *);

  std::string search(XcacheCommand *);

  void removeMeta(XcacheMeta *);

  void flush(XcacheMeta *);

  void makeRoom(XcacheMeta *);

  void status(void);

  bool alreadyHasMeta(XcacheMeta *);

  int32_t getContextId(void) {
    return contextID;
  };

  bool hasRoom(XcacheMeta *);
};

#endif

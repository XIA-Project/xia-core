#ifndef __MEMHT_H__
#define __MEMHT_H__

#include <map>
#include "store.h"

/**
 * MemHt:
 * @brief Content Store - [In-memory hash table]
 * In memory hash table using C++'s stl
 */

class MemHt:public XcacheContentStore {
private:
  std::map<XcacheMeta *, std::string> memht;

public:
  MemHt()
  {
  }

  int store(XcacheMeta *meta, std::string data)
  {
    std::cout << "Reached MemHt::" << __func__ << " Storing " << data << "\n";
    memht[meta] = data;

    return 0;
  }

  std::string get(XcacheMeta *meta)
  {
    std::cout << "Memht get\n";
    return memht[meta];
  }

  void print(void)
  {
    
  }
};

#endif

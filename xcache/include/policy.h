#ifndef __POLICY_H__
#define __POLICY_H__

#include "meta.h"

/**
 * XcachePolicy:
 * @brief Abstract class that defines a eviction policy
 * Xcache eviction policy decides which content object to evict. The policy module
 * does not need to care about actually storing the data. It only deals in terms of
 * "meta"data.
 */

class XcachePolicy  {
public:
  virtual int store(XcacheMeta *) {
    return 0;
  };
  virtual int get(XcacheMeta *) {
    return 0;
  };
  virtual int remove(XcacheMeta *) {
    return 0;
  };
  virtual XcacheMeta *evict() {
    return NULL;
  };
};

/**
 * All the policies are implemented in header files. The corresponding header files
 * must be included here.
 */
#include "policies/fifo.h"

#endif /* __POLICY_H__ */

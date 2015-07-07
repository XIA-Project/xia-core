#include "slice.h"
#include "meta.h"

#define IGNORE_PARAM(__param) ((void)__param)

XcacheMeta::XcacheMeta(XcacheCommand *cmd)
{
  refCount = 0;
  store = NULL;
  cid = cmd->cid();
}

XcacheMeta::XcacheMeta()
{
  refCount = 0;
  store = NULL;
}

void XcacheMeta::status(void)
{
  std::cout << "[" << cid << "] ref = " << refCount << "\n";
  std::cout << "\tDATA: [" << store->get(this) << "]\n";
}

void XcacheMeta::addedToSlice(XcacheSlice *slice)
{
  ref();
  sliceMap[slice->getContextId()] = slice;
}

void XcacheMeta::removedFromSlice(XcacheSlice *slice)
{
  IGNORE_PARAM(slice);
  deref();  
}


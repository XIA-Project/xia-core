#include "slice.h"
#include "meta.h"
#include "policy.h"

XcacheSlice::XcacheSlice(int32_t contextID)
{
  /* TODO: Policy is always FIFO */
  FifoPolicy fifo;

  maxSize = currentSize = ttl = 0;
  policy = fifo;

  this->contextID = contextID;

}

void XcacheSlice::addMeta(XcacheMeta *meta)
{
  metaMap[meta->getCid()] = meta;
  meta->addedToSlice(this);
}

bool XcacheSlice::hasRoom(XcacheMeta *meta)
{
  if(maxSize - currentSize >= meta->getLength())
    return true;
  return false;
}

void XcacheSlice::removeMeta(XcacheMeta *meta)
{
  std::map<std::string, XcacheMeta *>::iterator iter;

  iter = metaMap.find(meta->getCid());
  metaMap.erase(iter);
  meta->removedFromSlice(this);
}

void XcacheSlice::makeRoom(XcacheMeta *meta)
{
  while(!hasRoom(meta)) {
    XcacheMeta *toRemove = policy.evict();
    removeMeta(toRemove);
  }
}

bool XcacheSlice::alreadyHasMeta(XcacheMeta *meta)
{
  std::map<std::string, XcacheMeta *>::iterator iter;

  iter = metaMap.find(meta->getCid());
  if(iter != metaMap.end())
    return true;
  return false;
}

int XcacheSlice::store(XcacheMeta *meta)
{
  if(alreadyHasMeta(meta))
    return -1;

  makeRoom(meta);
  addMeta(meta);
  return policy.store(meta);
}

std::string XcacheSlice::search(XcacheCommand *cmd)
{
  std::map<std::string, XcacheMeta *>::iterator i;

  i = metaMap.find(cmd->cid());
  if(i != metaMap.end()) {
    return i->second->get();
  }

  return "";
}

void XcacheSlice::setPolicy(XcachePolicy policy)
{
  this->policy = policy;
}

void XcacheSlice::status(void)
{
  std::map<std::string, XcacheMeta *>::iterator i;

  std::cout << "Slice [" << contextID << "]\n";
  for(i = metaMap.begin(); i != metaMap.end(); ++i) {
    i->second->status();
  }
}

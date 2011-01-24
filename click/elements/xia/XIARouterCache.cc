#include <click/config.h>
#include "XIARouterCache.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/xiacontentheader.hh>
#include <click/string.hh>
#include <clicknet/xia.h>
#include <click/packet.hh>
#include <click/vector.hh>
#include <click/xid.hh>


CLICK_DECLS
XIARouterCache::XIARouterCache()
{
  timer=0;
 // oldPartial=contentTable;
  cache_content_from_network = true;
}
XIARouterCache::~XIARouterCache(){}

int 
XIARouterCache::configure(Vector<String> &conf, ErrorHandler *errh)
{
   Element* routing_table_elem;
//   std::cout<<"enter configure"<<std::endl;
   if (cp_va_kparse(conf, this, errh,
                "LOCAL_ADDR", cpkP+cpkM, cpXIAPath, &local_addr,
		"ROUTETABLENAME", cpkP+cpkM, cpElement, &routing_table_elem,
                "CACHE_CONTENT_FROM_NETWORK", cpkP, cpBool, &cache_content_from_network,
                cpEnd) < 0)
    return -1;   
 //std::cout<<"Route Table Name: "<<routing_table_name.c_str()<<std::endl;
    routeTable = dynamic_cast<XIAXIDRouteTable*>(routing_table_elem);
    hid=local_addr.xid(local_addr.destination_node());
    /*
    if(routeTable==NULL) std::cout<<"NULL routeTable"<<std::endl;
    if(ad_given)
    {
      std::cout<<"ad: "<<sad.c_str()<<std::endl;
      ad.parse(sad);
      std::cout<<ad.unparse().c_str()<<std::endl;
    }
    if(hid_given)
    {
      std::cout<<"hid: "<<shid.c_str()<<std::endl;
      hid.parse(shid);
      std::cout<<hid.unparse().c_str()<<std::endl;
    }
    */
//    std::cout<<"pkt size: "<<PKTSIZE<<std::endl;
    return 0;
}
int 
XIARouterCache::MakeSpace(int chunkSize)
{
      while( usedSize + chunkSize > MAXSIZE)
	{
	  HashTable<XID,int>::iterator i;
	  for(i=partial.begin();i!=partial.end();i++)
	  {
	    if(i->second==0)
	    {
	      break;
	    }
	  }
	  if(i!=partial.end())
	  {
	    HashTable<XID,CChunk*>::iterator iter = partialTable.find(i->first);
	    usedSize-=iter->second->GetSize();
	    
	    CChunk *t=iter->second;    
	    
	    partialTable.erase(iter);
	    partial.erase(i);
	    
	    delete t;
	    
	    continue;
	  }
	  for(i=content.begin();i!=content.end();i++)
	  {
	     if(i->second==0)
	     {
		break;
	     }
	  }
	  if(i!=content.end())
	  {
	     HashTable<XID,CChunk*>::iterator iter=contentTable.find(i->first);
	     usedSize-=iter->second->GetSize();
	     // modify the routin table	     //delete
	     delRoute(iter->first);	     	     
	     CChunk *t=iter->second;	     
	     contentTable.erase(iter);	          
	     content.erase(i);	     
	     delete t;	     
	     continue;
	  }
	  
	  HashTable<XID,CChunk*>::iterator iter=partialTable.begin();
	  if(iter!=partialTable.end())
	  {
	    usedSize-=iter->second->GetSize();	    
	    CChunk *t=iter->second;	    
	    partialTable.erase(iter);
	    partial.erase(i);	    
	    delete t;	    
	    continue;
	  }
	  
	  iter=contentTable.begin();
	  usedSize-=iter->second->GetSize();
	  //modify routing table	   //delete
	  delRoute(iter->first);	  
	  CChunk *t=iter->second;	  
	  contentTable.erase(iter);
	  content.erase(i);	  
	  delete t;	  
	}  
	return 0;
}

void XIARouterCache::push(int port, Packet *p)
{
//  std::cout<<"in "<<local_addr.unparse(this).c_str()<<std::endl;
//  std::cout<<"enter cache push"<<std::endl;
  const struct click_xia* hdr = p->xia_header();
  if (!hdr)
      return ;
  if (hdr->dnode == 0 || hdr->snode == 0)
      return ;
  
  XIAHeader xhdr(p);
  
  struct click_xia_xid _dstID =  hdr->node[hdr->dnode - 1].xid;
  uint8_t dst_xid_type = _dstID.type;
  XID dstID(_dstID);
// std::cout<<"dstID: "<<dstID.unparse().c_str()<<std::endl;
  struct click_xia_xid _srcID = hdr->node[hdr->dnode + hdr->snode - 1].xid;
  uint8_t src_xid_type = _srcID.type;
  XID srcID(_srcID);
//  std::cout<<"srcID: "<<srcID.unparse().c_str()<<std::endl;
  int cid_type;  
  cp_xid_type("CID", &cid_type);   
  
  
  if(src_xid_type==cid_type)  //store, this is chunk response
  {
// std::cout<<"srcID is CID"<<std::endl;
 //std::cout<<"content header"<<std::endl;
    ContentHeader ch(p);  
 //std::cout<<"*************"<<std::endl;
    const unsigned char *payload=xhdr.payload();
//std::cout<<"payload: "<<payload<<std::endl;
    int offset=ch.chunk_offset();
    int length=ch.length();
    int chunkSize=ch.chunk_length();   
//    std::cout<<"srcID is CID"<<std::endl;
//    std::cout<<"offset is "<<offset<<std::endl;
//    std::cout<<"length is "<<length<<std::endl;
//    std::cout<<"chunkSize is "<<chunkSize<<std::endl;
    
    if(dstID==hid)   //cache in client, should return the whole chunk    
    {
//      std::cout<<"dstID is myself"<<std::endl;
      HashTable<XID,CChunk*>::iterator it,oit;
      bool chunkFull=false;
      CChunk* chunk;
#ifdef CLIENTCACHE
      HashTable<XID,CChunk*>::iterator cit;
      cit=contentTable.find(srcID);
      if(cit!=contentTable.end())
      {
	content[srcID]=1;	
	p->kill();
	
	timer++;
	if(timer>=REFRESH)
	{
	  timer=0;
	  
	  for(it=oldPartial.begin();it!=oldPartial.end();it++)
	  {
	      chunk=it->second;
	      delete chunk;
	  }
	  oldPartial.clear();	  
	  oldPartial=partialTable;	  
	  for(it=partialTable.begin();it!=partialTable.end();it++)
	  {
	      chunk=it->second;
	      delete chunk;
	  }
	  partialTable.clear();	
	  cit=contentTable.begin();
	  while(cit!=contentTable.end())
	  {
	    if( content[cit->first]==0 )
	    {
	      HashTable<XID, CChunk*>::iterator pit=cit;
	      cit++;
	      oldPartial[pit->first]=pit->second;
	      delRoute(cit->first);
	      content.erase(pit->first);
	      contentTable.erase(pit);
	    }
	    else
	    {
	      content[cit->first]=0;
	    }
	  }
	}
	return ;
      }
#endif
      it=partialTable.find(srcID);
      if(it!=partialTable.end()) //already in partial table
      {
//std::cout<<"found in partial table"<<std::endl;
	chunk=it->second;
	chunk->fill(payload, offset, length);
	if(chunk->full())
	{
	  chunkFull=true;
	  partialTable.erase(it);
	}	
      }
      else
      {	
	oit=oldPartial.find(srcID);
	if(oit!=oldPartial.end())  //already in old partial table
	{
	  chunk=oit->second;
	  chunk->fill(payload, offset, length);
	  oldPartial.erase(oit);
	  if(chunk->full())
	  {
	    chunkFull=true;
	  }
	  else
	  {
	    partialTable[srcID]=chunk;
	  }
	}	
	else			//first pkt to the client
	{
//  std::cout<<"first pkt of a chunk"<<std::endl;
	  chunk=new CChunk(srcID, chunkSize);
	  chunk->fill(payload, offset, length);
	  if(chunk->full())
	  {
	    chunkFull=true;
	  }
	  else
	  {
	    partialTable[srcID]=chunk;
	  }
	}
      } 
      if(chunkFull)  //have built the whole chunk pkt
      {
//std::cout<<"chunk is complete"<<std::endl;
	XIAHeaderEncap encap;
	XIAHeader hdr(p);
	
	encap.set_dst_path(hdr.dst_path());      
	encap.set_src_path(hdr.src_path());	
	encap.set_plen(chunkSize);
	encap.set_nxt(CLICK_XIA_NXT_CID);
	
	ContentHeaderEncap  contenth(0, 0, 0, chunkSize);
	uint16_t hdrsize = encap.hdr_size()+ contenth.hlen();      	
	//build packet	
	WritablePacket *newp = Packet::make(hdrsize, chunk->GetPayload() , chunkSize, 20 );	
	
	newp=contenth.encap(newp);		// add XIA header
	newp=encap.encap( newp, false );	
//std::cout<<"made new full-chunk pkt, chunkSize is "<<chunkSize<<", hdrsize is "<<hdrsize<<std::endl;
//std::cout<<"In client"<<std::endl;
//std::cout<<"payload: "<<chunk->GetPayload()<<std::endl;
//std::cout<<"pushed to rpc"<<std::endl;
	checked_output_push(1 , newp);
//std::cout<<"have pushed out"<<std::endl;
#ifdef CLIENTCACHE
        if (cache_content_from_network)
        {
            contentTable[srcID]=chunk;
            content[srcID]=1;
            addRoute(srcID);
        }
        else
            delete chunk;
#endif
#ifndef CLIENTCACHE	
	delete chunk;
#endif	
      }
      // delete too old partial content
      timer++;
      if(timer>=REFRESH)
      {
	timer=0;
	
	for(it=oldPartial.begin();it!=oldPartial.end();it++)
	{
	    chunk=it->second;
	    delete chunk;
	}
	oldPartial.clear();	
	oldPartial=partialTable;	
	for(it=partialTable.begin();it!=partialTable.end();it++)
	{
	    chunk=it->second;
	    delete chunk;
	}
	partialTable.clear();
#ifdef CLIENTCACHE	
	cit=contentTable.begin();
	while(cit!=contentTable.end())
	{
	  if( content[cit->first]==0 )
	  {
	    HashTable<XID, CChunk*>::iterator pit=cit;
	    cit++;
	    oldPartial[pit->first]=pit->second;
	    delRoute(cit->first);
	    content.erase(pit->first);
	    contentTable.erase(pit);
	  }
	  else
	  {
	    content[cit->first]=0;
	  }
	}	
#endif
      }
      p->kill();            
    }
    //cache in server, router
    else
    {
//std::cout<<"dst is not myself"<<std::endl;
      HashTable<XID,CChunk*>::iterator it;
      it=contentTable.find(srcID);  
      if(port==1)  //put CID, server get the chunk
      {
//std::cout<<"In server: push CID"<<std::endl;
//std::cout<<"payload:"<<payload<<std::endl;
//std::cout<<"payload length: "<<length<<std::endl;
	if(it==contentTable.end())
	{
	  CChunk *chunk=new CChunk(srcID, chunkSize);
	  chunk->fill(payload, 0, chunkSize);
	  contentTable[srcID]=chunk;
	  addRoute(srcID);
	}
      }
      else    //router get the chunk packet
      {
//std::cout<<"In router: get a response pkt"<<std::endl;
//std::cout<<"payload:"<<payload<<std::endl;
	  if(it!=contentTable.end())    //already in contentTable
	  {
//printf("found in contentTable\n");
	    content[it->first]=1;
	  }
	  else
	  {
	    it=partialTable.find(srcID);
	    if(it!=partialTable.end())  //found in partialTable
	    {
//printf("found in partialTable\n");
//std::cout<<it->first.c_str()<<" "<<it->second
	      partial[it->first]=1;
	      CChunk *chunk=it->second;
	      chunk->fill(payload, offset, length);
	      if(chunk->full())
	      {
		contentTable[srcID]=chunk;
		content[srcID]=1;
		addRoute(srcID);
		partial.erase(it->first);
		partialTable.erase(it);
	      }
	    }
	    else                         //first pkt of a chunk
	    {
//printf("first pkt of a chunk\n");
	      CChunk *chunk=new CChunk(srcID, chunkSize);
	      chunk->fill(payload, offset, length);//  allocate space for new chunk	  
	      
	      MakeSpace(chunkSize);  //lru		
	      
	      if(chunk->full())
	      {
		contentTable[srcID]=chunk;
		content[srcID]=1;
		//modify routing table	  //add
		addRoute(srcID);
	      }
	      else
	      {
		partialTable[srcID]=chunk;
		partial[srcID]=1;
	      }	
	      usedSize += chunkSize;	
	    }
	  }
	  //lru refresh
//printf("lru refresh\n");
//std::cout<<timer<<std::endl;
	  timer++;
	  if(timer>=REFRESH)
	  {
	    HashTable<XID,int>::iterator iter;
	    for(iter=content.begin();iter!=content.end();iter++)
	    {
	      iter->second=0;
	    }
	    for(iter=partial.begin();iter!=partial.end();iter++)
	    {
	      iter->second=0;
	    }
	    timer=0;
	  }
      }
      p->kill();
//printf("end: dstID is not myself\n");
    }
  }
  else if(dst_xid_type==cid_type)  //look_up,  chunk request
  { 
//    std::cout<<"dstID is CID"<<std::endl;  
    HashTable<XID,CChunk*>::iterator it;
    it=contentTable.find(dstID);    
#ifdef CLIENTCACHE
    if(srcID==hid)
    {
      if(it!=contentTable.end())
      {
	content[dstID]=1;
	
	XIAHeaderEncap encap;
	XIAPath srcPath, dstPath;      
	char* pl=it->second->GetPayload();
	unsigned int s=it->second->GetSize();
	
	handle_t s_dummy=srcPath.add_node(XID());
	handle_t s_cid=srcPath.add_node(dstID);
	srcPath.add_edge(s_dummy, s_cid);
	
	handle_t d_dummy=dstPath.add_node(XID());
	handle_t d_hid=dstPath.add_node(srcID);
	dstPath.add_edge(d_dummy, d_hid);
	
	encap.set_src_path(srcPath);
	encap.set_dst_path(dstPath);
	encap.set_plen(s);
	encap.set_nxt(CLICK_XIA_NXT_CID);      
	
	ContentHeaderEncap  contenth(0, 0, 0, s);
	
	uint16_t hdrsize = encap.hdr_size()+ contenth.hlen();  
	WritablePacket *newp = Packet::make(hdrsize, pl , s, 20 );
	
	newp=contenth.encap(newp);
	newp=encap.encap( newp, false );	      // add XIA header
	checked_output_push(1 , newp);
//std::cout<<"In client"<<std::endl;
//std::cout<<"payload: "<<pl<<std::endl;
//std::cout<<"have pushed out"<<std::endl;
      }
      p->kill();  
      return ;
    }
#endif
    // server, router
    if(it!=contentTable.end())
    { 
//std::cout<<"look up cache in router or server"<<std::endl;
      XIAHeaderEncap encap;
      XIAHeader hdr(p);
      XIAPath myown_source;  // AD:HID:CID add_node, add_edge
      char* pl=it->second->GetPayload();
//std::cout<<"payload: "<<pl<<std::endl;
      unsigned int s=it->second->GetSize();
//std::cout<<"chunk size: "<<s<<std::endl;      
      myown_source = local_addr;
      handle_t _cid=myown_source.add_node(dstID);      
      myown_source.add_edge(myown_source.source_node(), _cid);
      myown_source.add_edge(myown_source.destination_node(), _cid);
      myown_source.set_destination_node(_cid);
      
      encap.set_src_path(myown_source);
      encap.set_dst_path(hdr.src_path());  
      encap.set_nxt(CLICK_XIA_NXT_CID);
      
      // add content header   dataoffset
      unsigned int cp=0;
      while(cp < s)
      {      
	int l= (s-cp) < PKTSIZE ? (s-cp) : PKTSIZE;
	ContentHeaderEncap  contenth(0, cp, l, s);
	uint16_t hdrsize = encap.hdr_size()+ contenth.hlen();      
	//build packet	
	WritablePacket *newp = Packet::make(hdrsize, pl + cp , l, 20 );	
	newp=contenth.encap(newp);	
	encap.set_plen(l);	// add XIA header
	newp=encap.encap( newp, false );	
	checked_output_push(0 , newp);
//std::cout<<"have pushed out"<<std::endl;
	cp += l;
      }
      p->kill();
    }
    else  //printf("dstID is not found in cache, pkt killed\n");
    {
//      std::cout<<"not found, kill pkt"<<std::endl;
      p->kill();
    }
  }  
  else
  {
    p->kill();
//std::cout<<"src and dst are not CID"<<std::endl;
  }
//std::cout<<"leave push"<<std::endl;
}

CPart::CPart(unsigned int off, unsigned int len)
{
  offset=off;
  length=len;
}

CChunk::CChunk(XID _xid, int chunkSize)
{
  size=chunkSize;
  complete=false;
  payload=new char[size];
  xid=_xid;  
}
CChunk::~CChunk()
{
  delete payload; 
}

void 
CChunk::Merge(std::list<CPart>::iterator it)
{
//std::cout<<"enter Merge: offset is "<< it->offset <<std::endl;
  std::list<CPart>::iterator post_it;
  post_it=it;
  post_it++;
  while(1)
  {
      if(post_it==parts.end()) break;
      if(post_it->offset > it->offset+it->length)
      {
	break;
      }
      else
      {
	 unsigned int l=it->offset+it->length;
	 unsigned int _l=post_it->offset+post_it->length;
	 if(l>_l)
	 {
	   parts.erase(post_it);
	 }
	 else
	 {
	   it->length= _l - it->offset;
	   parts.erase(post_it);
	 }
	 post_it=it;
	 post_it++;
      }      
  }  
}

int 
CChunk::fill(const unsigned char *_payload, unsigned int offset, unsigned int length)
{
//  std::cout<<"enter fill, payload is "<<_payload<<std::endl;
  std::list<CPart>::iterator it, post_it;
  CPart p(offset, length);  
  char *off = payload + offset ;
  for( it=parts.begin(); it!=parts.end(); it++ )
  {
    if(it->offset > offset)break;
  }
  if(it==parts.end())    //end
  {
    if(it==parts.begin())  //empty
    {
//std::cout<<"push back"<<std::endl;  
	memcpy(off, _payload, length);    
	
	parts.push_back(p);
    }
    else                //not empty
    {
      it--;
      if( it->offset + it->length < offset + length)
      {
	 memcpy(off, _payload, length);
//std::cout<<"off is "<<off<<std::endl;
//std::cout<<"push back"<<std::endl;
	 parts.push_back(p);
	 Merge(it);
      }
    }
  }
  else    //not end
  {
    if(it==parts.begin())  //begin
    {
      memcpy(off, _payload, length);
//std::cout<<"push front"<<std::endl;
      parts.push_front(p);
      it=parts.begin();
      Merge(it);
    }
    else		//not begin
    {
      it--;
      if(offset <= it->offset + it->length )  //overlap with previous interval
      {
	if( offset+length <= it->offset + it->length ) //cover
	{
	}
	else   //not cover
	{
	  memcpy(off, _payload, length);
	  it->length = (offset+length) - it->offset;
	  Merge(it);
	}	
      }
      else   // not overlap with previous interval
      {
	memcpy(off,_payload, length);
//std::cout<<"insert"<<std::endl;
	parts.insert(post_it, p);
	post_it=it;
	post_it++;
	Merge(post_it);	
      }
    }
  }  
//  std::cout<<"leave fill: list size is "<<parts.size()<<std::endl;
  return 0;
}
bool
CChunk::full()
{
  if(complete==true) return true;
  std::list<CPart>::iterator it;
  it= parts.begin();
  if( it->offset==0 && it->length==size )
  {
      complete=true;
      return true;
      
  }
  return false;  
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIARouterCache)
ELEMENT_MT_SAFE(XIARouterCache)

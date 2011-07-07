#include "xiacontentmodule.hh"
#include <click/xiacontentheader.hh>
#include <click/config.h>
#include <click/glue.hh>
CLICK_DECLS

XIAContentModule::XIAContentModule(XIATransport *transport) 
{
    _transport = transport;
    _timer=0;
} 

XIAContentModule::~XIAContentModule() 
{
} 

void XIAContentModule::process_request(Packet *p, const XID &srcID, const XID & dstCID)
{ 
    //    std::cout<<"dstID is CID"<<std::endl;  
    HashTable<XID,CChunk*>::iterator it;
    it=_contentTable.find(dstCID);    
#ifdef CLIENTCACHE
    if(srcID==_transport->local_hid())
    {
	if(it!=_contentTable.end())
	{
	    content[dstCID]=1;

	    XIAHeaderEncap encap;
	    XIAPath srcPath, dstPath;      
	    char* pl=it->second->GetPayload();
	    unsigned int s=it->second->GetSize();

	    handle_t s_dummy=srcPath.add_node(XID());
	    handle_t s_cid=srcPath.add_node(dstCID);
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
	    _transport->checked_output_push(1 , newp);
	    //std::cout<<"In client"<<std::endl;
	    //std::cout<<"payload: "<<pl<<std::endl;
	    //std::cout<<"have pushed out"<<std::endl;
	}
	p->kill();  
	return ;
    }
#endif
    // server, router
    if(it!=_contentTable.end())
    { 
	//std::cout<<"look up cache in router or server"<<std::endl;
	XIAHeaderEncap encap;
	XIAHeader hdr(p);
	XIAPath myown_source;  // AD:HID:CID add_node, add_edge
	char* pl=it->second->GetPayload();
	//std::cout<<"payload: "<<pl<<std::endl;
	unsigned int s=it->second->GetSize();
	//std::cout<<"chunk size: "<<s<<std::endl;      
	myown_source = _transport->local_addr();
	handle_t _cid=myown_source.add_node(dstCID);      
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
	    _transport->checked_output_push(0 , newp);
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

/* source ID is the content */
void XIAContentModule::cache_incoming(Packet *p, const XID& srcCID, const XID& dstID, int port)
{
    XIAHeader xhdr(p);  // parse xia header and locate nodes and payload
    ContentHeader ch(p);  

    const unsigned char *payload=xhdr.payload();
    int offset=ch.chunk_offset();
    int length=ch.length();
    int chunkSize=ch.chunk_length();   

    if(dstID==_transport->local_hid())   // cache in client, should return the whole chunk    
    {
	HashTable<XID,CChunk*>::iterator it,oit;
	bool chunkFull=false;
	CChunk* chunk;
#ifdef CLIENTCACHE
	HashTable<XID,CChunk*>::iterator cit;
	cit=_contentTable.find(srcCID);
	if(cit!=_contentTable.end())
	{
	    content[srcCID]=1;	
	    p->kill();

	    _timer++;
	    if(_timer>=REFRESH)
	    {
		_timer=0;

		for(it=oldPartial.begin();it!=oldPartial.end();it++)
		{
		    chunk=it->second;
		    delete chunk;
		}
		oldPartial.clear();	  
		oldPartial=_partialTable;	  
		for(it=_partialTable.begin();it!=_partialTable.end();it++)
		{
		    chunk=it->second;
		    delete chunk;
		}
		_partialTable.clear();	
		cit=_contentTable.begin();
		while(cit!=_contentTable.end())
		{
		    if( content[cit->first]==0 )
		    {
			HashTable<XID, CChunk*>::iterator pit=cit;
			cit++;
			oldPartial[pit->first]=pit->second;
			delRoute(cit->first);
			content.erase(pit->first);
			_contentTable.erase(pit);
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
	it=_partialTable.find(srcCID);
	if(it!=_partialTable.end()) //already in partial table
	{
	    //std::cout<<"found in partial table"<<std::endl;
	    chunk=it->second;
	    chunk->fill(payload, offset, length);
	    if(chunk->full())
	    {
		chunkFull=true;
		_partialTable.erase(it);
	    }	
	}
	else
	{	
	    oit=oldPartial.find(srcCID);
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
		    _partialTable[srcCID]=chunk;
		}
	    }	
	    else			//first pkt to the client
	    {
		//  std::cout<<"first pkt of a chunk"<<std::endl;
		chunk=new CChunk(srcCID, chunkSize);
		chunk->fill(payload, offset, length);
		if(chunk->full())
		{
		    chunkFull=true;
		}
		else
		{
		    _partialTable[srcCID]=chunk;
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
	    _transport->checked_output_push(1 , newp);
	    //std::cout<<"have pushed out"<<std::endl;
#ifdef CLIENTCACHE
	    if (_cache_content_from_network)
	    {
		_contentTable[srcCID]=chunk;
		content[srcCID]=1;
		addRoute(srcCID);
	    }
	    else
		delete chunk;
#endif
#ifndef CLIENTCACHE	
	    delete chunk;
#endif	
	}
	// delete too old partial content
	_timer++;
	if(_timer>=REFRESH)
	{
	    _timer=0;

	    for(it=oldPartial.begin();it!=oldPartial.end();it++)
	    {
		chunk=it->second;
		delete chunk;
	    }
	    oldPartial.clear();	
	    oldPartial=_partialTable;	
	    for(it=_partialTable.begin();it!=_partialTable.end();it++)
	    {
		chunk=it->second;
		delete chunk;
	    }
	    _partialTable.clear();
#ifdef CLIENTCACHE	
	    cit=_contentTable.begin();
	    while(cit!=_contentTable.end())
	    {
		if( content[cit->first]==0 )
		{
		    HashTable<XID, CChunk*>::iterator pit=cit;
		    cit++;
		    oldPartial[pit->first]=pit->second;
		    delRoute(cit->first);
		    content.erase(pit->first);
		    _contentTable.erase(pit);
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
	it=_contentTable.find(srcCID);  
	if (port==1)  //put CID, server get the chunk
	{
	    //std::cout<<"In server: push CID"<<std::endl;
	    //std::cout<<"payload:"<<payload<<std::endl;
	    //std::cout<<"payload length: "<<length<<std::endl;
	    if(it==_contentTable.end())
	    {
		CChunk *chunk=new CChunk(srcCID, chunkSize);
		chunk->fill(payload, 0, chunkSize);
		_contentTable[srcCID]=chunk;
		addRoute(srcCID);
	    }
	}
	else    //router get the chunk packet
	{
	    //std::cout<<"In router: get a response pkt"<<std::endl;
	    //std::cout<<"payload:"<<payload<<std::endl;
	    if (it!=_contentTable.end())    //already in contentTable
	    {
		//printf("found in contentTable\n");
		content[it->first]=1;
	    }
	    else
	    {
		it=_partialTable.find(srcCID);
		if(it!=_partialTable.end())  //found in partialTable
		{
		    //printf("found in partialTable\n");
		    //std::cout<<it->first.c_str()<<" "<<it->second
		    partial[it->first]=1;
		    CChunk *chunk=it->second;
		    chunk->fill(payload, offset, length);
		    if(chunk->full())
		    {
			_contentTable[srcCID]=chunk;
			content[srcCID]=1;
			addRoute(srcCID);
			partial.erase(it->first);
			_partialTable.erase(it);
		    }
		}
		else                         //first pkt of a chunk
		{
		    //printf("first pkt of a chunk\n");
		    CChunk *chunk=new CChunk(srcCID, chunkSize);
		    chunk->fill(payload, offset, length);//  allocate space for new chunk	  

		    MakeSpace(chunkSize);  //lru		

		    if(chunk->full())
		    {
			_contentTable[srcCID]=chunk;
			content[srcCID]=1;
			//modify routing table	  //add
			addRoute(srcCID);
		    }
		    else
		    {
			_partialTable[srcCID]=chunk;
			partial[srcCID]=1;
		    }	
		    usedSize += chunkSize;	
		}
	    }
	    //lru refresh
	    //printf("lru refresh\n");
	    //std::cout<<timer<<std::endl;
	    _timer++;
	    if(_timer>=REFRESH)
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
		_timer=0;
	    }
	}
	p->kill();
	//printf("end: dstID is not myself\n");
    }
}

int 
XIAContentModule::MakeSpace(int chunkSize)
{
    while( usedSize + chunkSize > MAXSIZE) {
	HashTable<XID,int>::iterator i;
	for(i=partial.begin();i!=partial.end();i++) 
	    if(i->second==0)
		break;
	

	if(i!=partial.end()) {
	    HashTable<XID,CChunk*>::iterator iter = _partialTable.find(i->first);
	    usedSize-=iter->second->GetSize();

	    CChunk *t=iter->second;    

	    _partialTable.erase(iter);
	    partial.erase(i);

	    delete t;
	    continue;
	}

	for(i=content.begin();i!=content.end();i++) {
	    if(i->second==0)
		break;
	}

	if(i!=content.end()) {
	    HashTable<XID,CChunk*>::iterator iter=_contentTable.find(i->first);
	    usedSize-=iter->second->GetSize();
	    // modify the routin table	     //delete
	    delRoute(iter->first);	     	     
	    CChunk *t=iter->second;	     
	    _contentTable.erase(iter);	          
	    content.erase(i);	     
	    delete t;	     
	    continue;
	}

	HashTable<XID,CChunk*>::iterator iter=_partialTable.begin();
	if(iter!=_partialTable.end()) {
	    usedSize-=iter->second->GetSize();	    
	    CChunk *t=iter->second;	    
	    _partialTable.erase(iter);
	    partial.erase(i);	    
	    delete t;	    
	    continue;
	}

	iter=_contentTable.begin();
	usedSize-=iter->second->GetSize();
	//modify routing table	   //delete
	delRoute(iter->first);	  
	CChunk *t=iter->second;	  
	_contentTable.erase(iter);
	content.erase(i);	  
	delete t;	  
    }  
    return 0;
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

void CChunk::Merge(CPartList::iterator it)
{
    //std::cout<<"enter Merge: offset is "<< it->offset <<std::endl;
    CPartList::iterator post_it;
    post_it=it;
    post_it++;

    while(1)
    {
	if(post_it==parts.end()) break;
	if(post_it->v.offset > it->v.offset+it->v.length)
	{
	    break;
	}
	else
	{
	    unsigned int l=it->v.offset+it->v.length;
	    unsigned int _l=post_it->v.offset+post_it->v.length;
	    if(l>_l)
	    {
		CPartListNode *n = post_it.get();
		parts.erase(post_it);
		delete n;
	    }
	    else
	    {
		it->v.length= _l - it->v.offset;
		CPartListNode *n = post_it.get();
		parts.erase(post_it);
		delete n;
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

    CPartList::iterator it, post_it;
    CPart p(offset, length);  
    char *off = payload + offset ;

    for( it=parts.begin(); it!=parts.end(); it++ )
    {
	if(it->v.offset > offset)break;
    }
    if(it==parts.end())    //end
    {
	if(it==parts.begin())  //empty
	{
	    //std::cout<<"push back"<<std::endl;  
	    memcpy(off, _payload, length);    
	    parts.push_back(new CPartListNode(p));
	}
	else                //not empty
	{
	    it--;
	    if( it->v.offset + it->v.length < offset + length)
	    {
		memcpy(off, _payload, length);
		//std::cout<<"off is "<<off<<std::endl;
		//std::cout<<"push back"<<std::endl;
		parts.push_back(new CPartListNode(p));
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
	    parts.push_front(new CPartListNode(p));
	    it=parts.begin();
	    Merge(it);
	}
	else		//not begin
	{
	    it--;
	    if(offset <= it->v.offset + it->v.length )  //overlap with previous interval
	    {
		if( offset+length <= it->v.offset + it->v.length ) //cover
		{
		}
		else   //not cover
		{
		    memcpy(off, _payload, length);
		    it->v.length = (offset+length) - it->v.offset;
		    Merge(it);
		}	
	    }
	    else   // not overlap with previous interval
	    {
		memcpy(off,_payload, length);
		//std::cout<<"insert"<<std::endl;
		parts.insert(post_it, new CPartListNode(p));
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

    CPartList::iterator it;
    it= parts.begin();

    if( it->v.offset==0 && it->v.length==size)
    {
	complete=true;
	return true;
    }
    return false;  
}

CLICK_ENDDECLS
//ELEMENT_REQUIRES(userlevel)
ELEMENT_PROVIDES(XIAContentModule)

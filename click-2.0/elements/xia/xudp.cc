#include <click/config.h>
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/packet.hh>
#include <click/vector.hh>

#include <click/xiacontentheader.hh>
#include "xiatransport.hh"
#include "xudp.hh"

#define DEBUG 0

CLICK_DECLS

XUDP::XUDP()
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;  
    _id=0;
    isConnected=false;
}

int
XUDP::configure(Vector<String> &conf, ErrorHandler *errh)
{
    XIAPath local_addr;
    Element* routing_table_elem;

    if (cp_va_kparse(conf, this, errh,
		"LOCAL_ADDR", cpkP+cpkM, cpXIAPath, &local_addr,
		"CLICK_IP", cpkP+cpkM, cpIPAddress, &_CLICKaddr,
		"API_IP", cpkP+cpkM, cpIPAddress, &_APIaddr,
		"ROUTETABLENAME", cpkP+cpkM, cpElement, &routing_table_elem,
		cpEnd) < 0)
	return -1;

    _local_addr = local_addr;
    _local_hid = local_addr.xid(local_addr.destination_node());
#if USERLEVEL
    _routeTable = dynamic_cast<XIAXIDRouteTable*>(routing_table_elem);
#else
    _routeTable = reinterpret_cast<XIAXIDRouteTable*>(routing_table_elem);
#endif

    return 0;
}

XUDP::~XUDP()
{

    //Clear all hashtable entries
    XIDtoPort.clear();
    portToDAGinfo.clear();
    portRxSeqNo.clear();
    portTxSeqNo.clear();

}



void XUDP::push(int port, Packet *p_input)
{    
    WritablePacket *p_in = p_input->uniqueify();
    //Depending on which CLICK-module-port it arrives at it could be control/API traffic/Data traffic

    switch(port){ // This is a "CLICK" port of UDP module.  
	case 0: //Control traffic
	    {
		//Extract the destination port 
		click_udp * uh=p_in->udp_header();

		//unsigned short _dport=uh->uh_dport;
		unsigned short _sport=uh->uh_sport;
		//click_chatter("control:%d",ntohs(_dport));

		p_in->pull(p_in->transport_header_offset());//Remove IP header
		p_in->pull(8); //Remove UDP header

		std::string p_buf;
		p_buf.assign((const char*)p_in->data(),(const char*)p_in->end_data());

		//protobuf message parsing
		xia_socket_msg.ParseFromString(p_buf);

		switch(xia_socket_msg.type()) {
		    case xia::XSOCKET:
			{
			    //Open socket. 
			    //click_chatter("\n\nOK: SOCKET OPEN !!!\\n");
			    portRxSeqNo.set(_sport,1);
			    portTxSeqNo.set(_sport,1);

			    // (for Ack purpose) Reply with a packet with the destination port=source port		
			    output(1).push(UDPIPEncap(p_in,_sport,_sport));	
			}
			break;
		    case xia::XBIND:
			{
			    //Bind XID
			    //click_chatter("\n\nOK: SOCKET BIND !!!\\n");
			    //get source DAG from protobuf message


			    xia::X_Bind_Msg *x_bind_msg = xia_socket_msg.mutable_x_bind();

			    String sdag_string(x_bind_msg->sdag().c_str(), x_bind_msg->sdag().size());

			    //String sdag_string((const char*)p_in->data(),(const char*)p_in->end_data());
			    //click_chatter("\nbind requested to %s, length=%d\n",sdag_string.c_str(),(int)p_in->length());

			    //Set source DAG info
			    DAGinfo daginfo;
			    daginfo.port=_sport;
			    //String str_local_addr=_local_addr.unparse();
			    //str_local_addr=str_local_addr+" "+xid_string;//Make source DAG _local_addr:SID
			    daginfo.src_path.parse(sdag_string);
			    daginfo.nxt=-1;
			    daginfo.last=-1;
			    daginfo.hlim=250;
			    daginfo.isConnected=false;
			    daginfo.initialized=false;

			    XID	source_xid = daginfo.src_path.xid(daginfo.src_path.destination_node());
			    //XID xid(xid_string);
			    //TODO: Add a check to see if XID is already being used
			    XIDtoPort.set(source_xid,_sport);//Maybe change the mapping to XID->DAGinfo?
			    addRoute(source_xid);

			    portToDAGinfo.set(_sport,daginfo);
			    //click_chatter("Bound");

			    portRxSeqNo.set(_sport,portRxSeqNo.get(_sport)+1);//Increment counter

			    // (for Ack purpose) Reply with a packet with the destination port=source port		
			    //output(1).push(UDPIPEncap(p_in,_sport,_sport));
			    break;
			}
		    case xia::XCLOSE:
			{
			    // Close port
			    //click_chatter("\n\nOK: SOCKET CLOSE !!!\\n");
			    XID source_xid=portToDAGinfo.get(_sport).xid;
			    delRoute(source_xid);
			    XIDtoPort.erase(source_xid);
			    portToDAGinfo.erase(_sport);
			    portRxSeqNo.erase(_sport);                
			    portTxSeqNo.erase(_sport);

			    // (for Ack purpose) Reply with a packet with the destination port=source port		
			    //output(1).push(UDPIPEncap(p_in,_sport,_sport));
			}
			break;
		    case xia::XCONNECT:
			{
			    //click_chatter("\n\nOK: SOCKET CONNECT !!!\\n");

			    isConnected=true;
			    //String dest((const char*)p_in->data(),(const char*)p_in->end_data());
			    //click_chatter("\nconnect to %s, length=%d\n",dest.c_str(),(int)p_in->length());

			    xia::X_Connect_Msg *x_connect_msg = xia_socket_msg.mutable_x_connect();

			    String dest(x_connect_msg->ddag().c_str());

			    //String sdag_string((const char*)p_in->data(),(const char*)p_in->end_data());
			    //click_chatter("\nbind requested to %s, length=%d\n",sdag_string.c_str(),(int)p_in->length());

			    XIAPath dst_path;
			    dst_path.parse(dest); 			

			    DAGinfo *daginfo=portToDAGinfo.get_pointer(_sport);

			    if(!daginfo) {
				//No local SID bound yet, so bind ephemeral one
				daginfo=new DAGinfo();
				daginfo->port=_sport;
				String str_local_addr=_local_addr.unparse_re();
				daginfo->isConnected=true;
				daginfo->initialized=true;

				String rand(click_random(1000000, 9999999));
				String xid_string="SID:20000ff00000000000000000000000000"+rand;
				str_local_addr=str_local_addr+" "+xid_string;//Make source DAG _local_addr:SID

				daginfo->src_path.parse_re(str_local_addr);

				daginfo->nxt=-1;
				daginfo->last=-1;
				daginfo->hlim=250;

				XID source_xid = daginfo->src_path.xid(daginfo->src_path.destination_node());

				XIDtoPort.set(source_xid,_sport);//Maybe change the mapping to XID->DAGinfo?
				addRoute(source_xid);
				portToDAGinfo.set(_sport,*daginfo);
			    }

			    daginfo=portToDAGinfo.get_pointer(_sport);
			    daginfo->dst_path=dst_path;
			    //click_chatter("\nbound to %s\n",portToDAGinfo.get_pointer(_sport)->src_path.unparse().c_str());

			    portRxSeqNo.set(_sport,portRxSeqNo.get(_sport)+1);//Increment counter

			    // (for Ack purpose) Reply with a packet with the destination port=source port		
			    //output(1).push(UDPIPEncap(p_in,_sport,_sport));
			}
			break;
		    case xia::XACCEPT:
			{
			    //click_chatter("\n\nOK: SOCKET ACCEPT !!!\\n");
			    DAGinfo *daginfo=portToDAGinfo.get_pointer(_sport);
			    daginfo->isConnected=true;
			    isConnected=true;
			    //p_in->kill();

			    // (for Ack purpose) Reply with a packet with the destination port=source port		
			    //output(1).push(UDPIPEncap(p_in,_sport,_sport));
			}
			break;

		    case xia::XGETSOCKETIDLIST:
			{
			    //Open socket. 
			    //click_chatter("\n\nOK: SOCKET OPEN !!!\\n");
			    int size = (int)portToDAGinfo.size();
			    //printf("size=%d \n", size);
			    xia::XSocketMsg xia_socket_msg;
			    xia_socket_msg.set_type(xia::XGETSOCKETIDLIST);
			    xia::X_Getsocketidlist_Msg *x_getsocketidlist_msg = xia_socket_msg.mutable_x_getsocketidlist();
			    x_getsocketidlist_msg->set_size(size);
			    int index = 0;
			    for (HashTable<unsigned short, DAGinfo>::iterator iter = portToDAGinfo.begin(); iter != portToDAGinfo.end(); ++iter ) {
				//printf("key=%d  \n", iter->first);
				//x_getsocketidlist_msg->set_id(index, (int)iter->first);
				x_getsocketidlist_msg->add_id((int)iter->first);
				index++;
			    }
			    std::string p_buf;
			    xia_socket_msg.SerializeToString(&p_buf);
			    WritablePacket *reply= WritablePacket::make(256, p_buf.c_str(), p_buf.size(), 0);
			    output(1).push(UDPIPEncap(reply,_sport,_sport));
			}
			break;

		    case xia::XGETSOCKETINFO:
			{
			    xia::X_Getsocketinfo_Msg *x_getsocketinfo_msg = xia_socket_msg.mutable_x_getsocketinfo();
			    int sockid = x_getsocketinfo_msg->id();	
			    HashTable<unsigned short, DAGinfo>::iterator iter = portToDAGinfo.find(sockid);

			    xia_socket_msg.set_type(xia::XGETSOCKETINFO);

			    x_getsocketinfo_msg->set_port((int)iter->second.port);

			    std::string p_buf;
			    xia_socket_msg.SerializeToString(&p_buf);
			    WritablePacket *reply= WritablePacket::make(256, p_buf.c_str(), p_buf.size(), 0);
			    output(1).push(UDPIPEncap(reply,_sport,_sport));
			}
			break;

		    default:
			click_chatter("\n\nERROR: CONTROL TRAFFIC !!!\n\n");
			break;

		}
	    }
	    break;

	case 1: //packet from Socket API
	    {
		if (DEBUG)
		    click_chatter("\nGot packet from socket");

		//Extract the destination port 
		const click_udp * uh=p_in->udp_header();

		//unsigned short _dport=uh->uh_dport;
		unsigned short _sport=uh->uh_sport;

		p_in->pull(p_in->transport_header_offset());//Remove IP header
		p_in->pull(8); //Remove UDP header

		std::string p_buf;
		p_buf.assign((const char*)p_in->data(),(const char*)p_in->end_data());
		//click_chatter("\n payload:%s, length=%d\n",p_buf.c_str(), p_buf.size());

		//protobuf message parsing
		xia_socket_msg.ParseFromString(p_buf);

		switch(xia_socket_msg.type()) { 
		    case xia::XSEND:
			{
			    if(!isConnected) {
				click_chatter("Not 'connect'ed");
			    } else {

				//click_chatter("\n\nOK: SOCKET DATA !!!\\n");

				xia::X_Send_Msg *x_send_msg = xia_socket_msg.mutable_x_send();

				String pktPayload(x_send_msg->payload().c_str(), x_send_msg->payload().size());

				int pktPayloadSize=pktPayload.length();

				//Find DAG info for that stream
				DAGinfo *daginfo=portToDAGinfo.get_pointer(_sport);

				//Recalculate source path
				XID	source_xid = daginfo->src_path.xid(daginfo->src_path.destination_node());
				String str_local_addr=_local_addr.unparse_re()+" "+source_xid.unparse();
				//Make source DAG _local_addr:SID
				if(isConnected) {
				    if(daginfo->src_path.unparse_re()!=str_local_addr) {
					//Moved!
					daginfo->src_path.parse_re(str_local_addr);
					//Send a control packet to transport on the other side
					//TODO: Change this to a proper format rather than use presence of extension header=1 to denote mobility
					class XIAHeaderEncap* _xiah=new XIAHeaderEncap();
					String str="MOVED";
					WritablePacket *p2 = WritablePacket::make (256, str.c_str(), str.length(),0);
					_xiah->set_nxt(22);
					_xiah->set_last(-1);
					_xiah->set_hlim(250);
					_xiah->set_dst_path(daginfo->dst_path);
					_xiah->set_src_path(daginfo->src_path);

					//Might need to remove more if another header is required (eg some control/DAG info)

					WritablePacket *p = NULL;
					p = _xiah->encap(p2, true);

					//click_chatter("Sent packet to network");
					output(2).push(p);
				    }


				    //Add XIA headers
				    class XIAHeaderEncap* _xiah=new XIAHeaderEncap();
				    if (daginfo->nxt >= 0) {
					_xiah->set_nxt(-1);
				    }
				    _xiah->set_last(-1);
				    _xiah->set_hlim(250);
				    _xiah->set_dst_path(daginfo->dst_path);
				    _xiah->set_src_path(daginfo->src_path);

				    if (DEBUG)
					click_chatter("sent packet to %s, from %s\n",daginfo->dst_path.unparse_re().c_str(),daginfo->src_path.unparse_re().c_str());


				    WritablePacket *just_payload_part= WritablePacket::make(p_in->headroom()+1, (const void*)x_send_msg->payload().c_str(), pktPayloadSize, p_in->tailroom());

				    WritablePacket *p = NULL;
				    p = _xiah->encap(just_payload_part, true);

				    //click_chatter("Sent packet to network");
				    output(2).push(p);

				    // (for Ack purpose) Reply with a packet with the destination port=source port	
				    // protobuf message
				    /*
				       xia::XSocketMsg xia_socket_msg_response;

				       xia_socket_msg_response.set_type(xia::XSOCKET_SENDTO);

				       std::string p_buf2;
				       xia_socket_msg.SerializeToString(&p_buf2);
				       WritablePacket *reply= WritablePacket::make(256, p_buf2.c_str(), p_buf2.size(), 0);
				       output(1).push(UDPIPEncap(reply,_sport,_sport));    
				     */	

				}
			    }
			}
			break;
		    case xia::XSENDTO:
			{
			    //click_chatter("\n\nOK: SOCKET SENDTO !!!\\n");

			    xia::X_Sendto_Msg *x_sendto_msg = xia_socket_msg.mutable_x_sendto();

			    String dest(x_sendto_msg->ddag().c_str(), x_sendto_msg->ddag().size());
			    String pktPayload(x_sendto_msg->payload().c_str(), x_sendto_msg->payload().size());



			    int dag_size = dest.length();
			    int pktPayloadSize=pktPayload.length();
			    //click_chatter("\n SENDTO ddag:%s, payload:%s, length=%d\n",xia_socket_msg.ddag().c_str(), xia_socket_msg.payload().c_str(), pktPayloadSize);

			    XIAPath dst_path;
			    dst_path.parse(dest); 

			    //Find DAG info for that stream
			    DAGinfo *daginfo=portToDAGinfo.get_pointer(_sport);

			    if(!daginfo) {
				//No local SID bound yet, so bind one  
				daginfo=new DAGinfo();
				daginfo->port=_sport;
				String str_local_addr=_local_addr.unparse_re();

				String rand(click_random(1000000, 9999999));
				String xid_string="SID:20000ff00000000000000000000000000"+rand;
				str_local_addr=str_local_addr+" "+xid_string;//Make source DAG _local_addr:SID

				daginfo->src_path.parse_re(str_local_addr);

				daginfo->nxt=-1;
				daginfo->last=-1;
				daginfo->hlim=250;

				XID	source_xid = daginfo->src_path.xid(daginfo->src_path.destination_node());

				XIDtoPort.set(source_xid,_sport);//Maybe change the mapping to XID->DAGinfo?
				addRoute(source_xid);
				portToDAGinfo.set(_sport,*daginfo);
			    }

			    //Recalculate source path
			    XID	source_xid = daginfo->src_path.xid(daginfo->src_path.destination_node());
			    String str_local_addr=_local_addr.unparse_re()+" "+source_xid.unparse();//Make source DAG _local_addr:SID
			    daginfo->src_path.parse_re(str_local_addr);

			    daginfo=portToDAGinfo.get_pointer(_sport);
			    portRxSeqNo.set(_sport,portRxSeqNo.get(_sport)+1);//Increment counter

			    if (DEBUG)
				click_chatter("sent packet to %s, from %s\n",dest.c_str(),daginfo->src_path.unparse_re().c_str());

			    //Add XIA headers
			    class XIAHeaderEncap* _xiah=new XIAHeaderEncap();
			    if (daginfo->nxt >= 0)
				_xiah->set_nxt(-1);
			    _xiah->set_last(-1);
			    _xiah->set_hlim(250);
			    _xiah->set_dst_path(dst_path);
			    _xiah->set_src_path(daginfo->src_path);


			    WritablePacket *just_payload_part= WritablePacket::make(p_in->headroom()+dag_size+1, (const void*)x_sendto_msg->payload().c_str(), pktPayloadSize, p_in->tailroom());

			    WritablePacket *p = NULL;

			    //click_chatter("Sent packet to network");
			    struct click_xia_xid _xid=dst_path.xid(dst_path.destination_node()).xid();
			    //printf("DSTTYPE:%s",dst_path.xid(dst_path.destination_node()).unparse().c_str());
			    //printf("DSTTYPE:%s",dst_path.xid(dst_path.destination_node()).unparse().c_str());

			    if(ntohl(_xid.type)==CLICK_XIA_XID_TYPE_CID) {
				ContentHeaderEncap  contenth(0, 0, 0, 0);
				p = contenth.encap(just_payload_part); 
				p = _xiah->encap(p, true);
			    } else {
				p = _xiah->encap(just_payload_part, true);
			    }
			    output(2).push(p);

			    // (for Ack purpose) Reply with a packet with the destination port=source port	
			    // protobuf message
			    /*
			       xia::XSocketMsg xia_socket_msg_response;

			       xia_socket_msg_response.set_type(xia::XSOCKET_SENDTO);

			       std::string p_buf2;
			       xia_socket_msg.SerializeToString(&p_buf2);
			       WritablePacket *reply= WritablePacket::make(256, p_buf2.c_str(), p_buf2.size(), 0);
			       output(1).push(UDPIPEncap(reply,_sport,_sport));    
			     */		        		

			}
			break;
		    case xia::XPUTCID:
			{
			    //click_chatter("\n\nOK: SOCKET PUTCID !!!\\n");
			    xia::X_Putcid_Msg *x_putcid_msg = xia_socket_msg.mutable_x_putcid();

			    String src(x_putcid_msg->sdag().c_str(), x_putcid_msg->sdag().size());
			    String pktPayload(x_putcid_msg->payload().c_str(), x_putcid_msg->payload().size());


			    //int dag_size = src.length();	
			    int pktPayloadSize=pktPayload.length();
			    //click_chatter("\n PUTCID sdag:%s, length=%d len=%d\n",x_putcid_msg->sdag().c_str(), pktPayloadSize, x_putcid_msg->payload().size());

			    XIAPath src_path;
			    src_path.parse(src); 

			    /*TODO: The destination dag of the incoming packet is local_addr:XID 
			     * Thus the cache thinks it is destined for local_addr and delivers to socket
			     * This must be ignored. Options
			     * 1. Use an invalid SID (done below)
			     * 2. The cache should only store the CID responses and not forward them to
			     *    local_addr when the source and the destination HIDs are the same.
			     * 3. Use the socket SID on which putCID was issued. This will
			     *    result in a reply going to the same socket on which the putCID was issued.
			     *    Use the response to return 1 to the putCID call to indicate success.
			     *    Need to add daginfo/ephemeral SID generation for this to work.
			     */
			    String dst_local_addr=_local_addr.unparse_re();							 		    	   
			    String xid_string="SID:0000000000000000000000000000000000000000";
			    dst_local_addr=dst_local_addr+" "+xid_string;//Make source DAG _local_addr:SID

			    XIAPath dst_path;
			    dst_path.parse_re(dst_local_addr);


			    //Add XIA headers
			    class XIAHeaderEncap* _xiah=new XIAHeaderEncap();
			    _xiah->set_last(-1);
			    _xiah->set_hlim(250);
			    _xiah->set_dst_path(dst_path);
			    _xiah->set_src_path(src_path);
			    _xiah->set_nxt(CLICK_XIA_NXT_CID);

			    //Might need to remove more if another header is required (eg some control/DAG info)

			    WritablePacket *just_payload_part= WritablePacket::make(256, (const void*)x_putcid_msg->payload().c_str(), pktPayloadSize, 0);			       

			    WritablePacket *p = NULL;
			    int chunkSize = pktPayloadSize;
			    ContentHeaderEncap  contenth(0, 0, pktPayloadSize, chunkSize);
			    p = contenth.encap(just_payload_part); 
			    p = _xiah->encap(p, true);

			    if (DEBUG)
				click_chatter("sent packet to cache");
			    output(3).push(p);

			    /*
			    // (for Ack purpose) Reply with a packet with the destination port=source port		
			    xia::XSocketMsg xia_socket_msg_response;

			    xia_socket_msg_response.set_type(xia::XSOCKET_PUTCID);

			    std::string p_buf1;
			    xia_socket_msg.SerializeToString(&p_buf1);
			    WritablePacket *reply= WritablePacket::make(256, p_buf1.c_str(), p_buf1.size(), 0);
			    output(1).push(UDPIPEncap(reply,_sport,_sport));
			     */ 

			}
			break;										
		    default:
			click_chatter("\n\nERROR DATA TRAFFIC !!!\n\n");
			break;

		} // inner switch   			
	    } // outer switch
	    break;

	case 2://Packet from network layer
	    {
		if (DEBUG)
		    click_chatter("Got packet from network");
		//Extract the SID/CID 
		XIAHeader xiah(p_in->xia_header());
		XIAPath dst_path=xiah.dst_path();
		XID	_destination_xid = dst_path.xid(dst_path.destination_node());    
		//TODO:In case of stream use source AND destination XID to find port, if not found use source. No TCP like protocol exists though
		//TODO:pass dag back to recvfrom. But what format?


		unsigned short _dport= XIDtoPort.get(_destination_xid);

		if(_dport)
		{
		    //TODO: Refine the way we change DAG in case of migration. Use some control bits. Add verification
		    DAGinfo daginfo=portToDAGinfo.get(_dport);

		    if(daginfo.initialized==false)
		    {
			daginfo.dst_path=xiah.src_path();
			daginfo.initialized=true;
			portToDAGinfo.set(_dport,daginfo);
		    }
		    if(xiah.nxt()==22&&daginfo.isConnected==true)
		    {
			//Verify mobility info
			daginfo.dst_path=xiah.src_path();
			portToDAGinfo.set(_dport,daginfo);
			p_in->kill();
			click_chatter("Sender moved, update to the new DAG");
		    }
		    //ENDTODO
		    else
		    {
			//Unparse dag info
			String src_path=xiah.src_path().unparse();
			String payload((const char*)xiah.payload(), xiah.plen());
			String str=src_path;
			str=str+String("^");
			str=str+payload;
			WritablePacket *p2 = WritablePacket::make (256, str.c_str(), str.length(),0);

			if (DEBUG)
			    click_chatter("Sent packet to socket");                			
			output(1).push(UDPIPEncap(p2,_dport,_dport));


			p_in->kill();
		    }
		}
		else
		{
		    click_chatter("Packet to unknown %s",_destination_xid.unparse().c_str());
		    p_in->kill();
		}
	    }

	    break;

	case 3://Packet from cache
	    {

		if (DEBUG)
		    click_chatter("Got packet from cache");
		//Extract the SID/CID 
		XIAHeader xiah(p_in->xia_header());
		XIAPath dst_path=xiah.dst_path();
		XID	_destination_xid = dst_path.xid(dst_path.destination_node());    

		unsigned short _dport= XIDtoPort.get(_destination_xid);

		if(_dport)
		{
		    //TODO: Refine the way we change DAG in case of migration. Use some control bits. Add verification
		    DAGinfo daginfo=portToDAGinfo.get(_dport);
		    daginfo.dst_path=xiah.src_path();
		    portToDAGinfo.set(_dport,daginfo);
		    //ENDTODO

		    //Unparse dag info
		    String src_path=xiah.src_path().unparse();
		    String payload((const char*)xiah.payload(), xiah.plen());
		    String str=src_path;

		    str=str+String("^");
		    str=str+payload;

		    WritablePacket *p2 = WritablePacket::make (256, str.c_str(), str.length(),0);

		    if (DEBUG)
			click_chatter("Sent packet to socket"); 


		    output(1).push(UDPIPEncap(p2,_dport,_dport));
		    p_in->kill();
		}
		else
		{
		    click_chatter("Packet to unknown %s",_destination_xid.unparse().c_str());
		    p_in->kill();
		}            

	    }
	    break;

    }

}

Packet *
XUDP::UDPIPEncap(Packet *p_in,int sport, int dport)
{
    WritablePacket *p = p_in->push(sizeof(click_udp) + sizeof(click_ip));
    click_ip *ip = reinterpret_cast<click_ip *>(p->data());
    click_udp *udp = reinterpret_cast<click_udp *>(ip + 1);

#if !HAVE_INDIFFERENT_ALIGNMENT
    assert((uintptr_t)ip % 4 == 0);
#endif
    // set up IP header
    ip->ip_v = 4;
    ip->ip_hl = sizeof(click_ip) >> 2;
    ip->ip_len = htons(p->length());
    ip->ip_id = htons(_id.fetch_and_add(1));
    ip->ip_p = IP_PROTO_UDP;
    ip->ip_src = _CLICKaddr;
    ip->ip_dst = _APIaddr;
    p->set_dst_ip_anno(IPAddress(_APIaddr));

    ip->ip_tos = 0;
    ip->ip_off = 0;
    ip->ip_ttl = 250;
    _cksum=false;

    ip->ip_sum = 0;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
    if (_aligned)
	ip->ip_sum = ip_fast_csum((unsigned char *)ip, sizeof(click_ip) >> 2);
    else
	ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));
#elif HAVE_FAST_CHECKSUM
    ip->ip_sum = ip_fast_csum((unsigned char *)ip, sizeof(click_ip) >> 2);
#else
    ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));
#endif

    p->set_ip_header(ip, sizeof(click_ip));

    // set up UDP header
    udp->uh_sport = sport;
    udp->uh_dport = dport;

    uint16_t len = p->length() - sizeof(click_ip);
    udp->uh_ulen = htons(len);
    udp->uh_sum = 0;
    if (_cksum) {
	unsigned csum = click_in_cksum((unsigned char *)udp, len);
	udp->uh_sum = click_in_cksum_pseudohdr(csum, ip, len);
    }

    return p;
}


enum {H_MOVE};

int XUDP::write_param(const String &conf, Element *e, void *vparam,
	ErrorHandler *errh)
{
    XUDP *f = static_cast<XUDP *>(e);
    switch(reinterpret_cast<intptr_t>(vparam)) {
	case H_MOVE: 
	{
	    XIAPath local_addr;
	    if (cp_va_kparse(conf, f, errh,
			"LOCAL_ADDR", cpkP+cpkM, cpXIAPath, &local_addr,
			cpEnd) < 0)
		return -1;
	    f->_local_addr = local_addr;
	    click_chatter("Moved to %s",local_addr.unparse().c_str());
	    f->_local_hid = local_addr.xid(local_addr.destination_node());

	} 
	break;
	default: break;
    }
    return 0;
}

void XUDP::add_handlers() {
    add_write_handler("local_addr", write_param, (void *)H_MOVE);
}


CLICK_ENDDECLS

EXPORT_ELEMENT(XUDP)
ELEMENT_REQUIRES(userlevel)
ELEMENT_REQUIRES(XIAContentModule)
ELEMENT_MT_SAFE(XUDP)

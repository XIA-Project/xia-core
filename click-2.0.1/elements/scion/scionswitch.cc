/*
** Copyright 2013 Carnegie Mellon University / ETH Zurich
** 
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
** 
** http://www.apache.org/licenses/LICENSE-2.0 
** 
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <sys/time.h>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>


/*change this to corresponding header*/
#include"scionswitch.hh"


CLICK_DECLS
int SCIONSwitch::configure(Vector<String> &conf, ErrorHandler *errh){
    if(cp_va_kparse(conf, this, errh, 
        "CONFIG_FILE", cpkM, cpString, &m_sConfigFile,
        "TOPOLOGY_FILE", cpkM, cpString, &m_sTopologyFile, 
        cpEnd) <0){

    }

    return 0;
}

int SCIONSwitch::initialize(ErrorHandler* errh){
   initVariables();
    _timer.initialize(this); 
    _timer.schedule_now();
    return 0;
}

void SCIONSwitch::run_timer(Timer* timer){
   //SL: Map each port to the address of the connected element
   //what if a gateway is directly connected to the switch?
   //need to add iNumGateways to the numPort; 	
   
   int numPort = m_iNumRouters+m_iNumServers+m_iNumClients+m_iNumGateways;
	
	#ifdef _SL_DEBUG
   //SLT: test
   printf("Switch (%llu): NumPort = %d, nRouters = %d, nServers = %d, nClients = %d\n", 
   		m_uAdAid, numPort, m_iNumRouters, m_iNumServers, m_iNumClients);
   printf("Switch (%llu) sending AID_REQ  %d\n", m_uAdAid, numPort);
   #endif
   
   //AD ID is set to the Src Addr for logging purpose.
   HostAddr s_addr(HOST_ADDR_SCION, m_uAdAid);
	
	#ifdef _SL_DEBUG
   //SLT
   printf("Switch (%llu): HostAddr:SRC = %llu\n", m_uAdAid, s_addr.numAddr());
   printf("Switch (%llu): HostAddr:SRC type = %d, Len = %d\n", 
   		m_uAdAid, s_addr.getType(), s_addr.getLength());
   #endif

   for(int i=0;i<numPort;i++){
		#ifdef _SL_DEBUG
		//SLT:
		printf("%dth Port: sending AID request\n", i);
   		#endif
		
		//SL: check if MAX_HOST_ADDR_SIZE is okay.
        uint8_t buf[COMMON_HEADER_SIZE+MAX_HOST_ADDR_SIZE];
        
		memset(buf, 0, COMMON_HEADER_SIZE+MAX_HOST_ADDR_SIZE);
        SPH::setType(buf, AID_REQ);
		SPH::setSrcAddr(buf, s_addr);
		SPH::setTotalLen(buf, COMMON_HEADER_SIZE+MAX_HOST_ADDR_SIZE);
        Packet* outPacket = Packet::make(DEFAULT_HD_ROOM, buf,
            COMMON_HEADER_SIZE+MAX_HOST_ADDR_SIZE, DEFAULT_TL_ROOM);
        output(i).push(outPacket);

		#ifdef _SL_DEBUG
		//SLT:
		printf("Switch (%llu) -- %dth Port: finishing AID request\n", m_uAdAid,i);
		#endif
   }
}

/*
SL: handle received packets from input ports
*/
void SCIONSwitch::push(int port, Packet* pkt){
    
    uint8_t type = SPH::getType((uint8_t*)pkt->data());
    uint16_t packetLength =
        SPH::getTotalLen((uint8_t*)pkt->data());
    //click_chatter("switch %d: %d, %d", m_uAdAid, pkt->length(), packetLength);
    uint8_t packet[packetLength];
    memset(packet, 0, packetLength);
    memcpy(packet, pkt->data(), packetLength);
    pkt->kill();
  
  	//SL: getInterface returns the egress interface id.
	//interface is used to forward a packet to the egress interface specified in its opaque field
    uint16_t o_interface = SPH::getOutgoingInterface(packet);
    

	//interface -> destination router address -> corresponding switch port
	//SL: itr1 (i.e., ifid -> aid translation) should be removed after having routers perform the translation
	//for this, a special (destination) field is required... in order to avoid IP encapsulation
	if(type == DATA) {
		#ifdef _SL_DEBUG_SW
		//SLT:
		printf("Switch (%llu): received a data packet: to IF:%d\n", m_uAdAid, o_interface);
		#endif

		std::map<uint16_t, HostAddr>::iterator itr1;
        itr1 = ifid2addr.find(o_interface);
        if (itr1!=ifid2addr.end() || !o_interface) {
			HostAddr target;
			if(!o_interface){
				target = SPH::getDstAddr(packet);
				printf("Packet will be sent to DST: %llu\n",target.numAddr());
            } else 
				target = itr1->second;
            //SLT: Map: class as a key problem
            std::map<HostAddr, int>::iterator itr2;
			itr2 = addr2port.find(target);
            if (itr2==addr2port.end()) {
                scionPrinter->printLog(EH,type,"Cannot Forward Packet to aid=%u\n",
                        itr2->second);
            } else {
                sendPacket(packet, packetLength, (int)itr2->second);
            }
        } else {
            scionPrinter->printLog(EH,type,"Cannot Forward Packet to ifid=%u\n",
                o_interface);
		}
	//control packets
	//1. Local: destination address -> port
	//2. OW: interface -> destination router address -> port
    } else {
		#ifdef _SL_DEBUG_SW
		//SLT:
		printf("Switch (%llu): received a control packet <type: %d>\n", m_uAdAid, type);
		#endif

		if(type > TO_LOCAL_ADDR) {

            HostAddr addr = SPH::getDstAddr(packet);
    	    int port;
			//SLT: Map
			std::map<HostAddr,int>::iterator itr = addr2port.find(addr);
			if(itr == addr2port.end()){

				#ifdef _SL_DEBUG_SW
				printf("\t in AD%llu: type:%d -- address (%llu) has not been registered in addr2port\n", m_uAdAid, type, addr.numAddr());
				#endif
				if(type != PATH_REG) {
					addToPendingTable(packet, type, packetLength, addr);
				}
				return;
			}
			port = itr->second;
			
			#ifdef _SL_DEBUG_SW
			//SLT:
			printf("Switch (%llu): a control packet <type: %d> would be sent to port %d\n", m_uAdAid, type,port);
			#endif

        	sendPacket(packet,packetLength,port);
		//2. Non-local control packets
		} else if (type!=AID_REP) {
			//SLT: temporary hack... for PCB
			if(type == BEACON) {
				o_interface = SCIONBeaconLib::getInterface(packet);
				#ifdef _SL_DEBUG
				printf("Outgoing Interface for PCB: %d\n",o_interface);
				#endif
			}
			std::map<uint16_t, HostAddr>::iterator itr1;
    	    itr1 = ifid2addr.find(o_interface);
        	if (itr1==ifid2addr.end()) {
			#ifdef _SL_DEBUG
			printf("ifid2addr mapping for IF[%d] is not found. (ifid2addr size: %d)\n",o_interface, ifid2addr.size());
			#endif
            	scionPrinter->printLog(EH,type,"Cannot Forward Packet to ifid=%u\n",
                	o_interface);
	        } else {
    	        //SLT: Map
				std::map<HostAddr, int>::iterator itr2;
        	    itr2 = addr2port.find(itr1->second);
            	if (itr2==addr2port.end()) {
					#ifdef _SL_DEBUG_CS
					printf("addr2port mapping for Addr[%llu] is not found. (addr2port size: %d)\n", itr1->second.numAddr(), addr2port.size());
					for(itr2 = addr2port.begin(); itr2 != addr2port.end(); itr2++) {
						printf("addr2port: addr = unknown,  port = %d\n", itr2->second);
					}
					#endif
                	scionPrinter->printLog(EH,type,"Cannot Forward Packet to aid=%u\n",
                        itr2->second);
       	     	} else {
        	        sendPacket(packet, packetLength, (int)itr2->second);
            	}
      	  	}
		//3. AID_REPLY...
		//very infrequent: only performed when AID is requested during the bootstrapping
		
		} else {
    		HostAddr addr = SPH::getSrcAddr(packet);
			//SLT:
			//Map: a key as a class... isn't operator== enough for find()?
			//this should be resolved...
    	    addr2port.insert(pair<HostAddr, int>(addr, port));
	
    	    if(addr==m_servers.find(CertificateServer)->second.addr){
        	    std::multimap<int, uint8_t*>::iterator itr;
            	for(itr=pendingCS.begin();itr!=pendingCS.end();itr++){
                	sendPacket(itr->second, itr->first, port); 
            	}
	        }else if(addr==m_servers.find(PathServer)->second.addr){
    	        std::multimap<int, uint8_t*>::iterator itr;
        	    for(itr=pendingPS.begin();itr!=pendingPS.end();itr++){
            	    sendPacket(itr->second, itr->first, port); 
           	 	}
	        }else if(addr==m_servers.find(BeaconServer)->second.addr){
    	        std::multimap<int, uint8_t*>::iterator itr;
        	    for(itr=pendingBS.begin();itr!=pendingBS.end();itr++){
            	    sendPacket(itr->second, itr->first, port); 
      	      	}
        	}

		}
	} //End of non-data (control) packet handling
	#ifdef _SL_DEBUG
	//SLT:
	printf("Switch (%llu): exiting push()\n", m_uAdAid);
	#endif

}

void SCIONSwitch::addToPendingTable(uint8_t * packet, uint8_t & type, uint16_t & len, HostAddr & addr) {
    uint16_t packetLength = SPH::getTotalLen(packet);
	if(type == ROT_REP_LOCAL){
        printf("Switch Received ROT reply to BS, yet BS port is not active\n");
        uint8_t* ptr = (uint8_t*)malloc(len);
        pair<int, uint8_t*> newPair = pair<int, uint8_t*>(packetLength, ptr);
        memcpy(newPair.second, packet, len);
        pendingBS.insert(newPair);
    }else if(type == ROT_REQ_LOCAL){
        printf("Switch Received ROT request, yet CS port is not active\n");
        uint8_t* ptr = (uint8_t*)malloc(packetLength);
        pair<int, uint8_t*> newPair = pair<int, uint8_t*>(packetLength, ptr);
        memcpy(newPair.second, packet, packetLength);
        pendingCS.insert(newPair);
    }
}

void SCIONSwitch::parseTopology(){
    TopoParser parser;
    parser.loadTopoFile(m_sTopologyFile.c_str()); 
    parser.parseServers(m_servers);
    parser.parseRouters(m_routers);
    parser.parseClients(m_clients);
    parser.parseGateways(m_gateways);
    parser.parseIFID2AID(ifid2addr);
    m_iNumRouters = parser.getNumRouters();
    m_iNumServers = parser.getNumServers();
    m_iNumClients = parser.getNumClients();
    m_iNumGateways = parser.getNumGateways();
}


void SCIONSwitch::initVariables(){
    Config config;
    config.parseConfigFile((char*)m_sConfigFile.c_str());
    config.getSwitchLogFilename(m_csLogFile);
    parseTopology();
    m_uAdAid = config.getAdAid();
    m_iLogLevel = config.getLogLevel();    
    scionPrinter = new SCIONPrint(m_iLogLevel, m_csLogFile);
}

void SCIONSwitch::sendPacket(uint8_t* data, uint16_t dataLength, int port ){
    uint16_t type = SPH::getType(data);
    HostAddr src = SPH::getSrcAddr(data);
    HostAddr dst = SPH::getDstAddr(data);
    uint32_t ts = SPH::getTimestamp(data);

    scionPrinter->printLog(IH,type,ts,src,dst,"%u,SENT\n",dataLength);
    WritablePacket *outPacket =
        Packet::make(DEFAULT_HD_ROOM,data,dataLength,DEFAULT_TL_ROOM);
    
	#ifdef _SL_DEBUG
	//SLT
	printf("Switch (%llu): send packet (%dB==%dB) to port %d\n",m_uAdAid, dataLength, outPacket->length(), port);
	#endif

    //click_chatter("switch %d: packet sent: %d", m_uAdAid, outPacket->length());
    output(port).push(outPacket);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SCIONSwitch)



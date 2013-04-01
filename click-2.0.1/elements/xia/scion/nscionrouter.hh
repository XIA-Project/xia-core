/*****************************************
 * File Name : sample.hh

 * Author : Sangjae Yoo <sangjaey@gmail.com>

 * Date : 08-02-2012

 * Purpose : 

******************************************/
#ifndef SCIONROUTER_HEADER_HH_
#define SCIONROUTER_HEADER_HH_

#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/timer.hh>
#include <click/notifier.hh>
#include <click/task.hh>
#include <map>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <sys/time.h>
/*include here*/
#include "packetheader.hh"
#include "scionbeacon.hh"
#include "topo_parser.hh"
#include "config.hh"
#include "scionprint.hh"
CLICK_DECLS

class NSCIONRouter : public Element { 
    public :
        NSCIONRouter():_timer(this){
            m_iCore = 0;
            m_iLogLevel = 300;
        };
        ~NSCIONRouter()
        {
            delete scionPrinter;
        };
       
        //const char *flow_code()  const {return "x/x";}
        const char *class_name() const {return "NSCIONRouter";}
        const char *port_count() const {return "-/-";}


        /*
            const char *processing():
                Configures the functionality of the ports. 
                returns input/output      

            Return values X/X
                Options for X
                h: push
                l: pull
                a: agnostic -> can be either push/pull but not both
            
            Available names for common processing
                
                AGNOSTIC        a/a
                PUSH            h/h
                PULL            l/l
                PUSH_TO_PULL    h/l
                PULL_TO_PUSH    l/h
                PROCESSING_A_AH a/ah 
                    : all input ports are agnostic/output[0] is agnostic and rest of
                    the output port is push

        */


        const char *processing() const {return PUSH;}

	private:
        void sendPacket(uint8_t* data, uint16_t dataLength, int port);
        
        int configure(Vector<String> &, ErrorHandler *);

        int initialize(ErrorHandler* errh);

        void run_timer(Timer *); 
        
        void push(int, Packet*); 

        bool getOfgKey(uint32_t timestamp, aes_context &actx);
       
        uint16_t getIFID(int port);
        
        void initVariables();

        void mapIFID2Port();

        void parseTopology();

        int verifyUpPacket(uint8_t* pkt, uint32_t ts);

        int verifyUpPacketCore(uint8_t* pkt, uint32_t ts);
    
        int verifyDownPacket(uint8_t* pkt, uint32_t ts);

        int verifyDownPacketCore(uint8_t* pkt, uint32_t ts);

        int verifyUpPeer(uint8_t* pkt, uint32_t ts);

        int verifyDownPeer(uint8_t* pkt, uint32_t ts);

        int verifyDownPacketEnd(uint8_t* pkt, uint32_t ts);
        
        int verifyDownPeerEnd(uint8_t* pkt, uint32_t ts);

		//SL: new functions
		int writeToEgressInterface(int port, uint8_t * pkt);

		int forwardDataPacket(int port, uint8_t * pkt);

		int processControlPacket(int port, uint8_t * pkt);

        int verifyInterface(int port, uint8_t* packet, uint8_t dir);

		void constructIfid2AddrMap();

		//SL:
		//verify opaque field (not crossover point)
		//this should handle uppath and downpath OFs differently based on the current Flag bit
		int verifyOF(int port, uint8_t* pkt); //verify opaque field (not crossover point)

		//SL:
		bool initOfgKey();
		bool updateOfgKey();

		//SLN:
		int normalForward(uint8_t type, int port, uint8_t * packet, uint8_t uppath);
		int crossoverForward(uint8_t type, uint8_t info, int port, uint8_t * packet, uint8_t uppath);

    private:
      
        Timer _timer;    
        
        map<uint16_t, uint16_t> ifid_map; 
        vector<uint16_t> ifid_set;
        map<int, uint16_t> port2ifid;
        map<uint16_t,int> ifid2port;
		//SLT
		map<uint16_t, HostAddr> ifid2addr;
        
        int m_iNumIFID; 
        String m_sConfigFile;
        String m_sTopologyFile;
        char m_csLogFile[MAX_FILE_LEN];
        std::map<uint32_t, ofgKey> key_table;
        std::map<uint32_t, aes_context*> m_OfgAesCtx;
        std::multimap<int, ServerElem> m_servers;
        std::multimap<int, RouterElem> m_routers;
        std::multimap<int, GatewayElem> m_gateways;         

		ofgKey m_currOfgKey;
		ofgKey m_prevOfgKey;
		
        uint8_t m_uMkey[16];
        uint64_t m_uAid;
        uint64_t m_uAdAid;
        uint32_t m_uTdAid;
        int m_iCore;
        int m_iLogLevel;
        SCIONPrint* scionPrinter;
};

CLICK_ENDDECLS


#endif

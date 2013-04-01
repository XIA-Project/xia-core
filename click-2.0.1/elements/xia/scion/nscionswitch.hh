/*****************************************
 * File Name : sample.hh

 * Author : Sangjae Yoo <sangjaey@gmail.com>

 * Date : 08-02-2012

 * Purpose : 

******************************************/
#ifndef SCIONSWITCH_HEADER_HH_
#define SCIONSWITCH_HEADER_HH_

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
#include <sys/time.h>
/*include here*/
#include "packetheader.hh"
#include "scionbeacon.hh"
#include "topo_parser.hh"
#include "config.hh"
#include "scionprint.hh"
#include "define.hh"

CLICK_DECLS

class NSCIONSwitch : public Element { 
    public :
        NSCIONSwitch(): _timer(this) {
            m_iNumClients =0;
            };
        ~NSCIONSwitch(){delete scionPrinter;};
       
//        const char *flow_code()  const {return "x/x";}
        const char *class_name() const {return "NSCIONSwitch";}
        const char *port_count() const {return "-/-";}

        void push(int, Packet*); 
        
        
        void run_timer(Timer *timer);
        
        const char *processing() const {return PUSH;}

        int configure(Vector<String> &, ErrorHandler *);

        int initialize(ErrorHandler* errh);

        void initVariables(); 

        void parseTopology();
        
        void sendPacket(uint8_t* data, uint16_t dataLength, int port);

		void addToPendingTable(uint8_t* pkt, uint8_t & type, uint16_t & len, HostAddr & addr);

    private:
        Timer _timer;
        String m_sConfigFile;
        String m_sTopologyFile;

        //Temporary Value

        int m_iNumRouters;
        int m_iNumServers;
        int m_iNumClients;
        int m_iNumGateways;

        char m_csLogFile[MAX_FILE_LEN];

        std::map<uint16_t, HostAddr> ifid2addr;
        //SLT: Map
		std::map<HostAddr, int> addr2port;   
        //std::map<uint64_t, int> addr2port;   
        
        std::multimap<int, ServerElem> m_servers;
        std::multimap<int, RouterElem> m_routers;
        std::multimap<int, GatewayElem> m_gateways;         
        std::map<int,ClientElem> m_clients;
        
        std::multimap<int, uint8_t*> pendingBS;
        std::multimap<int, uint8_t*> pendingPS;
        std::multimap<int, uint8_t*> pendingCS;
        
        int m_iClientPort;
        int m_iLogLevel;
        uint64_t m_uAdAid;
        SCIONPrint* scionPrinter;
};

CLICK_ENDDECLS


#endif

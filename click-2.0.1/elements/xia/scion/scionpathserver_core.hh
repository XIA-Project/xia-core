/*****************************************
 * File Name : sample.hh

 * Author : Sangjae Yoo <sangjaey@gmail.com>

 * Date : 08-02-2012

 * Purpose : 

******************************************/
#ifndef SCIONPATHSERVER_CORE_HEADER_HH_
#define SCIONPATHSERVER_CORE_HEADER_HH_

#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/timer.hh>
#include <click/notifier.hh>
#include <click/task.hh>
#include <map>
#include <vector>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
/*include here*/
#include "packetheader.hh"
#include "scionbeacon.hh"
#include "topo_parser.hh"
#include "config.hh"
#include "scionpathserver.hh"
CLICK_DECLS

struct path{
    uint16_t hops;
    uint16_t numPeers;
    std::vector<uint64_t>path;
    std::multimap<uint64_t, uint64_t> peers;
    uint16_t pathLength;
    uint64_t destAID;
    uint16_t tdid;
    uint32_t timestamp;
    uint8_t* msg;
};


class SCIONPathServerCore : public Element { 
    public :
        SCIONPathServerCore():_task(this){};
        ~SCIONPathServerCore(){delete scionPrinter;};
       
//        const char *flow_code()  const {return "x/x";}
        const char *class_name() const {return "SCIONPathServerCore";}
        const char *port_count() const {return "-/-";}
        
        const char *processing() const {return PULL_TO_PUSH;}

        int configure(Vector<String> &, ErrorHandler *);

        int initialize(ErrorHandler* errh);

        bool run_task(Task *);

       // void push(int, Packet*); 

        void parsePath(uint8_t* pkt);

        void printPaths();

        void reversePath(uint8_t* path, uint8_t* output, uint8_t hops);

        void initVariables();
        
        void sendPacket(uint8_t* packet, uint16_t packetLength, int port);
        void constructIfid2AddrMap();
        void parseTopology();
    private:
        Task _task;
        uint64_t m_uAid;
        uint64_t m_uAdAid;
		//std::multimap<uint64_t, path> paths;
        std::multimap<uint64_t, std::multimap<uint32_t, path> > paths;
        int m_iTval;
        char m_sLogFile[MAX_FILE_LEN];
        int m_iLogLevel;
        int m_iNumRegisteredPath;
        String m_sConfigFile;
        String m_sTopologyFile;
        SCIONPrint* scionPrinter;
        std::map<uint16_t, HostAddr> ifid2addr;
        std::multimap<int, ServerElem> m_servers;
        std::multimap<int, RouterElem> m_routers;
};

CLICK_ENDDECLS


#endif

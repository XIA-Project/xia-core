#ifndef SCION_CERT_CORE_HEADER_HH_
#define SCION_CERT_CORE_HEADER_HH_

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

/*include here*/

#include "packetheader.hh"
#include "scionbeacon.hh"
#include "rot_parser.hh"
#include "topo_parser.hh"
#include "config.hh"
#include "scionprint.hh"

CLICK_DECLS

class SCIONCertServerCore : public Element { 
    public :
        SCIONCertServerCore():_timer(this), _task(this){};
        ~SCIONCertServerCore(){delete scionPrinter;};
       
        const char *class_name() const {return "SCIONCertServerCore";}
        const char *port_count() const {return "-/-";}
        const char *processing() const {return PULL_TO_PUSH;}

        int configure(Vector<String> &, ErrorHandler *);
        int initialize(ErrorHandler* errh);
        bool run_task(Task *task);

        void parseTopology();
        void parseROT();

        void sendPacket(uint8_t* data, uint16_t dataLength, int port);
        void getCertFile(uint8_t* fn, uint64_t target);
        //int  verifyCert(uint8_t* pkt);
        void reversePath(uint8_t* path, uint8_t* output, uint8_t hops);
        void sendROT();

    private:
	
        Task _task;
        Timer _timer; 
        
        uint64_t m_uAid;
        uint64_t m_uAdAid;                  //AD AID of pcb server
        uint64_t m_uTdAid;
        
	// logger
	int m_iLogLevel;
        char m_csLogFile[MAX_FILE_LEN];
        SCIONPrint* scionPrinter;

	// configuration file parameters
        String m_sConfigFile;
        String m_sTopologyFile;
        String m_sROTFile;
	String m_csCert;        //temp certificate file path
        String m_csPrvKey;      //temp private key file path

	// RoT strcutrues
        ROT rot;
        char* curROTRaw;
        int curROTLen;


        std::map<uint64_t, int> certRequests;
        std::multimap<int, ServerElem> m_servers;
        std::multimap<int, RouterElem> m_routers;
        std::multimap<int, GatewayElem> m_gateways;         
};

CLICK_ENDDECLS


#endif

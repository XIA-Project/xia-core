#ifndef SCION_CERT_HEADER_HH_
#define SCION_CERT_HEADER_HH_

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

class SCIONCertServer : public Element { 
    public :
        SCIONCertServer():_timer(this), _task(this){};
        ~SCIONCertServer(){delete scionPrinter;};
       
	/* click element related */
        const char *class_name() const {return "SCIONCertServer";}
        const char *port_count() const {return "-/-";}
        const char *processing() const {return PULL_TO_PUSH;}
		bool run_task(Task *task);
        int configure(Vector<String> &, ErrorHandler *);
        int initialize(ErrorHandler* errh);

	/* parsing works */
	private:
        void parseTopology();
        void parseROT();

        void sendPacket(uint8_t* data, uint16_t dataLength, int port);

        void getCertFile(uint8_t* fn, uint64_t target);

        int verifyCert(uint8_t* pkt);

        void reversePath(uint8_t* path, uint8_t* output, uint8_t hops);

        int sendROT(uint32_t ROTVersion, HostAddr srcAddr);

        int isRequested(uint64_t, HostAddr);

        void constructIfid2AddrMap();

		//SLN:
		void processROTRequest(uint8_t * packet);
		void processCertificateRequest(uint8_t * packet);
		void processCertificateReply(uint8_t * packet);
		void processLocalCertificateRequest(uint8_t * packet);

    private:
        Task _task;
        Timer _timer; 
        
        uint64_t m_uAid;
        uint64_t m_uAdAid;                  //AD AID of pcb server
        uint64_t m_uTdAid;
        int m_iLogLevel;
	
	/* config files location */
        String m_sConfigFile;
        String m_sTopologyFile;
        String m_sROTFile;
		String m_csCert;        //temp certificate file path
		String m_csPrvKey;      //temp private key file path
	
	/* ROT objects */
        ROT rot;
        char* curROTRaw;
        int curROTLen;

        char m_csLogFile[MAX_FILE_LEN];
		SCIONPrint* scionPrinter;
        
        std::multimap<uint32_t, uint64_t> rotRequests;
        std::multimap<uint64_t, HostAddr> certRequests; //for childeren AD
        std::multimap<int, ServerElem> m_servers;
        //SL: unused elements but can be used later for key/cert. distribution
		/////////////////////////////////////////////////////////////////////
		std::multimap<int, RouterElem> m_routers;
        std::multimap<int, GatewayElem> m_gateways;         
		/////////////////////////////////////////////////////////////////////
        std::multimap<int, HostAddr> ifid2addr;

};

CLICK_ENDDECLS


#endif

#ifndef SCIONBEACONSERVER_CORE_HEADER_HH_
#define SCIONBEACONSERVER_CORE_HEADER_HH_

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
#include "topo_parser.hh"
#include "scionprint.hh"
#include "packetheader.hh"
#include "scionbeacon.hh"
#include "config.hh"
#include "rot_parser.hh"
#include "define.hh"

CLICK_DECLS


class SCIONBeaconServerCore : public Element {
	public :
		SCIONBeaconServerCore(): _timer(this), _task(this), _CryptoIsReady(false), _AIDIsRegister(false), m_iPCBGenPeriod(1){};
		~SCIONBeaconServerCore()
		{
			if(_CryptoIsReady) rsa_free(&PriKey);
			delete scionPrinter;
		};
		
		const char *class_name() const {return "SCIONBeaconServerCore";}
		const char *port_count() const {return "-/-";} // any # of input ports / any # of output ports
		const char *processing() const {return PULL_TO_PUSH;} // same as "l/h"

		/* click related functions */
		int configure(Vector<String> &, ErrorHandler *);
		int initialize(ErrorHandler* errh);
		void run_timer(Timer *timer);
	        bool run_task(Task *task);
        
        	void sendPacket(uint8_t* data, uint16_t dataLength, int port);	// send packet to nscionswitch
		bool getOfgKey(const uint32_t &timestamp, aes_context &actx);
		bool generateNewPCB();	// PCB packet generation

		bool initRsaCtx(rsa_context* rctx);
   
		//SL:
		bool initOfgKey();
		bool updateOfgKey();
	private:
		bool parseROT();
		void parseTopology();
		void loadPrivateKey();
		//SL:
		void updateIfidMap(uint8_t * packet);
		
		bool m_bROTInitiated;

		std::map<uint32_t, aes_context*> m_OfgAesCtx; 
		
		Timer _timer;
		Task _task;

		// RoT structure	
		ROT m_cROT;

		String m_sConfigFile;				//configuration file name
		String m_sTopologyFile;				//topology file name
		String m_sROTFile;					// ROT file
		char m_csCertFile[MAX_FILE_LEN];	//certificate file name
		char m_csPrvKeyFile[MAX_FILE_LEN];	//private key file name
		char m_csLogFile[MAX_FILE_LEN];     //log file name   

		uint64_t m_uAid;					//AID (address)
		uint64_t m_uAdAid;					//Administrative Ddomain AID (AD #)
		uint64_t m_uTdAid;					//Trust Domain AID
		uint8_t m_uMkey[MASTER_SECRET_KEY_SIZE];				//Master secret key

		//cryptography object, now we use RSA
	   	rsa_context PriKey;
		
		// flags
		bool m_bRsaCtxLoad;
		bool _CryptoIsReady;
		bool _AIDIsRegister;

		int m_iLogLevel;
		SCIONPrint* scionPrinter;				
		
		// Period for Sending PCB packets
		int m_iPCBGenPeriod;
        
		// OFG keys
		ofgKey m_currOfgKey;
		ofgKey m_prevOfgKey;
		
		// topology structures
		std::map<uint16_t, uint16_t> ifid_map;	//map between local ifid and neighbor ifid
		std::multimap<int, ServerElem> m_servers;
		std::multimap<int, RouterElem> m_routers;
};

CLICK_ENDDECLS


#endif



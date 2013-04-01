#ifndef SCIONBEACONSERVER_HEADER_HH_
#define SCIONBEACONSERVER_HEADER_HH_

#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/timer.hh>
#include <click/notifier.hh>
#include <click/task.hh>
#include <map>
#include <queue> //added by HC
#include <tr1/unordered_set> //added by HC
#include <tr1/unordered_map> //added by HC
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

/*include here*/
#include "packetheader.hh"
#include "scionbeacon.hh"
#include "topo_parser.hh"
#include "rot_parser.hh"
#include "config.hh"
#include "scionprint.hh"
#include "define.hh"

CLICK_DECLS
/*
TODO:
        1. Define data structre necessary for PCB's (up path, down path)
        2. Accepts PCB whenever it comes in..... 
        3. Uplodad PCB to Core only when it is necessary. 
        4. Has a table of PCB
        5. 
*/


//HC: forward declaration
class SelectionPolicy;

class SCIONBeaconServer : public Element { 
    public :
        SCIONBeaconServer(): _timer(this) ,_task(this), 
	  	_CryptoIsReady(false), _AIDIsRegister(false), m_iIsRegister(0), 
	  	m_iNumRegisteredPath(0), m_iBeaconTableSize(300){};

        ~SCIONBeaconServer()
        {
	  		if(_CryptoIsReady)rsa_free(&PriKey);
	  		clearCertificateMap();
	  		clearAesKeyMap();
	  		delete scionPrinter;
        };
        
        /*CLICK functions*/ 
        const char *class_name() const {return "SCIONBeaconServer";}
        const char *port_count() const {return "-/-";}
        const char *processing() const {return PULL_TO_PUSH;}
        int configure(Vector<String> &, ErrorHandler *);
        int initialize(ErrorHandler* errh);
        void run_timer(Timer *timer);
        bool run_task(Task *task);
        /*****/

	private:
        void parseTopology(); 
        bool parseROT();
        
        uint8_t verifyPcb(uint8_t* pkt);
        
        int registerPaths();
        int registerPath(pcb &rpcb);
        int propagate();
        int buildPath(uint8_t* pkt, uint8_t* output);
        
        scionHash createHash(uint8_t* pkt);        
        
        void addPcb(uint8_t* pkt);
        void addUnverifiedPcb(uint8_t* pkt);
	
        void constructIfid2AddrMap(); 
        void printPaths();
        void print(uint8_t* path, int hops); 
        void sendPacket(uint8_t* data, uint16_t dataLength, int port);
        void getCertFile(uint8_t* fn, uint64_t target);

        void loadPrivateKey();
       
        void recheckPcb();
        
        bool getOfgKey(uint32_t timestamp, aes_context &actx);
        
        uint16_t removeSignature(uint8_t* inPacket, uint8_t* outPacket);
        
        void requestForCert(uint8_t* packet);
		void clearCertificateMap();
		void clearAesKeyMap();
		bool getPathToRegister(std::multimap<uint32_t, pcb>::reverse_iterator &itr);

		//SL:
		bool initOfgKey();
		bool updateOfgKey();

		//HC: initialize selection policy
    	void initSelectionPolicy();

		//SLN:
		void requestROT();
		void requestROT(uint8_t * packet);
		void processPCB(uint8_t * packet, uint16_t packetLength);
		void saveCertificate(uint8_t * packet, uint16_t packetLength);
		void updateIfidMap(uint8_t * packet);
		void saveROT(uint8_t * packet, uint16_t packetLength);
		void sendAIDReply(uint8_t * packet, uint16_t packetLength);

    private:
        Timer _timer;
        Task  _task;        
        uint64_t m_uAdAid;                  //AD AID of pcb server
        uint64_t m_uTdAid;                    //TDID
        uint64_t m_uAid;                     //AID of pcb server

        /*Temporary Values*/
        bool m_bROTInitiated;
		bool _CryptoIsReady;
		bool _AIDIsRegister;
                
        char m_csCertFile[MAX_FILE_LEN];
        char m_csPrvKeyFile[MAX_FILE_LEN];
        char m_csLogFile[MAX_FILE_LEN];

        String m_sConfigFile;
        String m_sTopologyFile;
        String m_sROTFile;

        int m_iFval;
        int m_iKval;
        int m_iPval;
        int m_iRegTime;
        int m_iPropTime;
        int m_iResetTime;
        int m_iScheduleTime;
        int m_iRecheckTime;
        int m_iLogLevel;
        int m_iIsRegister;  
        int m_iNumRegisteredPath;
        int m_iBeaconTableSize;
        
	rsa_context PriKey;
	
        ROT m_cROT;
                
        uint8_t m_uMkey[MASTER_SECRET_KEY_SIZE];

        time_t m_lastPropTime;
        time_t m_lastRegTime;
        time_t m_lastRecheckTime;

	ofgKey m_currOfgKey;
	ofgKey m_prevOfgKey;
		
        std::map<uint16_t, uint16_t> ifid_map;
        std::map<uint16_t, HostAddr> ifid2addr;
        std::map<uint32_t, ofgKey> key_table;
        std::map<uint32_t, aes_context *> m_OfgAesCtx;
	//SL: use a pointer instead of an object if element is big
	//yet seems to be okay if not frequently accessed
	//for infrequently accessed objects, memory management can cause more overhead.
        std::multimap<uint32_t, pcb> beacon_table;
        std::multimap<uint32_t, pcb> unverified_pcbs;
        std::multimap<uint32_t, pcb> k_paths;
	//////////////////////////////////////////////////////////
        std::map<uint64_t, int> certRequest;
        std::multimap<int, ServerElem> m_servers;
        std::multimap<int, RouterElem> m_routers;
        std::multimap<int, GatewayElem> m_gateways;         
	std::map<uint64_t, x509_cert*> m_certMap;
        SCIONPrint* scionPrinter;
        
    	//HC: selection policy per customer (identified by interfaceID)
    	std::map<uint16_t, SelectionPolicy> m_selPolicies;

};

// HC: path selection policy
// Basically we consider two policy types: (1) exclusion and (2)
// prioritization. Exclusion policies specify paths that should be
// avoid, and prioritization policies specify the preference between
// paths.
//Current implementation: 
//- Exclusion policy: maximum age, maximum path length, min elapsed time, and unwanted ADs.
//- Prioritization policy: weight of age (curtime - timestamp), weight
//  of path length, weight of freshness (curtime - last_seen)
// TODO: 
//- implement a more comprehensive policy framework that includes, for
// example, path disjointness
class SelectionPolicy {
public:
    SelectionPolicy(int numSel) : m_iNumSelectedPath(numSel) {
        //initialization (should be overwritten in initSelectionPolicy)
        setExclusion(20, 8, 5, std::tr1::unordered_set<uint64_t>());
        setWeight(1.0, 0.0, 0.0);
        setSelectionProbability(0.9);
    }
    //the lower the better
    std::vector<pcb*> select(const std::multimap<uint32_t, pcb> &availablePaths); 
    bool setExclusion(time_t, uint8_t, time_t, const std::tr1::unordered_set<uint64_t>&);
    bool setWeight(double, double, double);
    bool setSelectionProbability(double);
    void printPcb(const pcb*); //for test
private:
    struct PathPriority{
        double priority;
        const pcb* cpcb;
        bool operator<(const struct PathPriority& other) const{
            return priority < other.priority;
        }
        PathPriority(double pri, const pcb* p) : priority(pri), cpcb(p){}
    };
    //define the hash and compare functions for unordered_set<scionHash>
    class mySetHash{
    public:
        ::std::size_t operator ()(const scionHash &h) const {
            size_t xorValue = 0; 
            for(int i = 0; i < SHA1_SIZE; ++i){
                xorValue ^= h.hashVal[i];
            }
            return xorValue; 
        };
    };
    class mySetEqual{
    public:
        bool operator ()(const scionHash &h1, const scionHash &h2) const {return (h1 == h2);};
    };
        
    bool isExcluded(time_t curTime, const pcb& cpcb);
    double computePriority(time_t curTime, const pcb& cpcb);
    void updateDigest(const pcb*);   
    scionHash createHash(uint8_t* pkt);
    size_t m_iNumSelectedPath;
    double m_dSelProb; //probability to select a pcb
    //exclusion list
    time_t m_tMaxAge;
    time_t m_tMinElapsedTime;
    uint8_t m_iMaxLen;
    std::tr1::unordered_set<uint64_t> m_excludedADs;
    //weight of path characteristics for computing priority
    double m_dAgeWT;
    double m_dLenWT;
    double m_dElapsedTimeWT; //freshness
    //internal state: e.g., digest of recently selected paths, which
    //is useful in computing inter-path characteristics such as
    //disjointness. Not used for now.
    std::tr1::unordered_map<scionHash, time_t, mySetHash, mySetEqual> digest;
};



CLICK_ENDDECLS


#endif


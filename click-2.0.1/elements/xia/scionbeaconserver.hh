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
#include "scionipencap.hh"

// added by Tenma
#ifdef ENABLE_AESNI
#include <intel_aesni/iaesni.h>
#endif

CLICK_DECLS
/*
TODO:
        1. Define data structure necessary for PCB's (up path, down path)
        2. Accepts PCB whenever it comes in..... 
        3. Uplodad PCB to Core only when it is necessary. 
        4. Has a table of PCB
        5. 
*/


//HC: forward declaration
class SelectionPolicy;

/** 
    @brief SCION Beacon Server element class.  

    This class is the implementation of the SCION Beacon Server. SCION Beacon
    Server has two types, the TDC Beacon Server and Non-TDC Beacon Server. This
    class is the implementation for the Non-TDC Beacon Server. The reference for
    the TDC Beacon Server can be found in scionbeaconserver_core.hh. 

    The Non-TDC Beacon Server (Beacon Server) performs two main operations. 

    1. Verifying received PCB
    
    Each AD verifies a received PCB before the AD adds its opaque field(s) to the PCB
	and propagates 	the PCB to its customers (i.e., announces path). 
	For PCB verification, the Beacon Server in the AD verifies all signatures 
	included in the PCB. The BeaconServer class uses the SCIONBeaconLib class 
	which implements all the necessary functions for the PCB manipulation. 
	You can refer to scionbeacon.hh for more details of the Beacon processing functions. 
    
    All the received PCBs passed to the verifyPCB() function in the beacon
    library. When the validation passes, the beacon is stored inside a queue for
    future use. If the PCB does not pass the verification, the PCB is either
    discarded or stored in a temporary queue depending on the reason of the
    failure.
    
    If the verification of the PCB fails due to the absence of necessary certificates
    (or possible ROT/certificate change), the PCB is stored to the temporary queue 
	for later verification. The Beacon Server requests missing certificates to 
	the Certificate Server. 
    
    If the verification fails due to invalid signature, the PCB is discarded.  

    2. Adding links to the PCB. 

    If PCB verification passes, the Beacon Server processes beacons as follows.
    The process is mainly adding ingress and egress interfaces to the PCB to help
	endpoints to construct a path to TDC. In addition to ingress and egress interfaces, 
	peering links are added to the PCB to enable shortcut path construction.
	For more information about the links, please see the documentation for the 
	SCIONBeaconLib class. 

    After the PCBs are verified and all necessary information is added, the
    Beacon Server signs the PCB and propagates it to customer ADs and local Path
	Servers (to provide an up-path). Also, the Beacon Server selects down-paths 
	among all received PCBs, and register them to TDC Path Server.

    @note any comments or documentation regarding the click router is not present
    in this documentation
*/
class SCIONBeaconServer : public Element { 
    public :
        /**
            @brief SCION Beacon Server Constructor.
            
            This is the constructor of the Beacon server. It initializes the timer
            and task value (click router fields) and the _CryptoIsReady,
            _AIDIsRegister, m_iIsRegister, m_iNUmRegisterPath, and
            m_iBeaconTableSize values.  
        */
        SCIONBeaconServer(): _timer(this) ,_task(this), 
	  	_CryptoIsReady(false), _AIDIsRegister(false), m_iIsRegister(0), 
	  	m_iNumRegisteredPath(0), m_iBeaconTableSize(300),
		m_bROTRequested(0){};

        /**
            @brief SCION Beacon Server Deconstructor.
        */
        ~SCIONBeaconServer()
        {
	  		if(_CryptoIsReady)rsa_free(&PriKey);
	  		clearCertificateMap();
	  		clearAesKeyMap();
	  		delete scionPrinter;
        };
        
        /* click related functions */
        const char *class_name() const {return "SCIONBeaconServer";}
        const char *port_count() const {return "-/-";} // any # of input ports / any # of output ports
        const char *processing() const {return PUSH;} // same as "h/h"

        int configure(Vector<String> &, ErrorHandler *);
        int initialize(ErrorHandler* errh);
        void run_timer(Timer *timer);
        bool run_task(Task *task);
        void push(int port, Packet *p);

        void sendHello();
        void getEgressIngressXIDs(vector<string> &list);

	private:
        /**
            @brief Parse Topology information using TopoParser. 

            This function parses the topology file using the TopoParser element.
            The path of the topology file is defined in the .conf file. The
            m_sTopologyFile value is set to this path in the initVariable()
            function. Only when a valid path is set, this function will extract
            the topology information from the TopoParser into the m_servers and
            m_routers structures.
            
            @note This function must be called after all the internal variables
            are initialized by initVariable() function.    
        */
        void parseTopology(); 

        /**
            @brief parse ROT information.
            @return true when success, false otherwise.
        
            This function parses the ROT file using the ROTParser element.
            The path of the topology file is defined in the .conf file. The
            m_sROTFile value is set to this path in the initVariable()
            function. Only when a valid path is set, this function will extract
            the ROT information from the ROTParser into the m_cROT structure. 
            
            @note This function must be called after all the internal variables
            are initialized by initVariable() function.    
        */
        bool parseROT(String filename = "");
        
        /**
            @brief verify Certificates in PCB.
            @param uint8_t* pkt PCB to be verified.
            @return SCION_SUCCESS when success, SCION_FAILURE otherwise.
        
            This function is called everytime when a new PCB is received. All the
            new PCBs must be verified by the Beacon server as described in the
            class details. This function is responsible of verifying PCBs and
            putting PCBs into the correct queue.
            
            The function returns SCION_SUCCESS on success, and SCION_FAILURE on
            error including those errors due to lack of certificates.   
        */
        uint8_t verifyPcb(uint8_t* pkt);
        
        /**
            @brief Registers selected paths (multiple).
       
            This funtion is a wrapper function for registerPath() function. It
            iterates through the PCB queue which is the beacon_table and calls
            registerPath() function for each paths until the number of the
            registered path reaches the maximum capacity of registered path. 
            
            Each Beacon Server has different number that limits the number of
            paths that can be registered.  
        */
        int registerPaths();

        /**
            @brief register given path (single).
            @param pcb &rpcb PCB to be registered.
            
            This function registers the given pcb (param) to the TDC path server.
            Before sending registeration packet to the TDC, it adds required
            information to the PCB. PCBs after getting verified by the verifyPcb()
            function, is sent to the beacon_table which is the queue for all the
            verified beacons. All the PCBs inside this table does not include the
            link or markings of the current AD. However, in order for the path
            registeration to work properly, the path inside the registeration
            packet should have the information of the current AD. 
            
            In short, the current AD must put the marking that it wants to include
            before sending the registeration packet. It is the same routine that
            propagate() function goes through.
            
            Also, the signatures are removed so that the size of the registeration
            packet is minimized.   
        */
        int registerPath(pcb &rpcb);
        /**
            @brief propagate selected PCB to downstream.
            @return Returns 0 when error, and SCION_SUCCESS on success. 

            This function iterates through the beacon table, chooses some PCBs to
            propagate and propagtes the selected PCBs after adding links to the
            selected PCBs. The following steps describe the details of the
            routine. 

            1. Selecting PCB from the Beacon Table. 
            
            The beacons are selecting by using the beacon selecting policy engine
            (TBD)
            
            2. Marking Step

            All the selected PCBs does not have the necessary marking of the
            current AD. This function adds the Markings for the up-down links and
            peering links. The detailed description of different typs of links are
            described in scionbeacon.hh. Depending on the policy, markings are
            added to the PCBs before propagation. 
            
            3. Signing the PCB
            
            After the markings are added to the PCBs, each PCBs are signed with
            the private key of the AD.
            
            When all three steps are done successfully, the PCBs are stored into a
            click packet and gets forwared to the next server or router.   
        */
        int propagate();
        /**
            @brief Builds an UP path using the PCB information. 
            @param uint8_t* pkt The buffer that contains the PCB. 
            @param uint8_t* output The buffer that will store the path. 
        
            This function builds an up-path by using the information of the given
            PCB packet. The output path contains all the opaque fields of the path
            and the special opaque field that contains the timestamp value.
            
            The returned path can be directly put into a DATA packet to forward
            DATA packets.    
        */
        int buildPath(uint8_t* pkt, uint8_t* output);
        
        /**
            @brief UNUSED
        */
        scionHash createHash(uint8_t* pkt);        
        
        /**
            @brief Adds the given PCB to the beacon_table
            @param uint8_t* pkt The buffer that contains the PCB. 
        
            This function adds the given PCB to the beacon_table. This function
            assumes that the given PCB is already verified using the verifyPCB()
            function. The PCBs that are not verified should not be passed into
            this function but they should be passed into addUnverifiedPcb()
            function or just be discarded. 

            @note This function will not misbehave even if the unverified PCBs are
            passed. However, the SCION system will not work in a secure way. 
        */
        void addPcb(uint8_t* pkt);
        /**
            @brief temporary add unverified pcb to the set. 
            @param uint8_t* pkt The buffer that contains the PCB. 
            
            Adds the PCB to the temporary queue. The PCBs that are not able to be
            verified are stored inside a queue so that it can be verfied when all
            the necessary certificates are available. 
        */
        void addUnverifiedPcb(uint8_t* pkt);
	
        /**
            @brief construct IFID/ADDR mapping.

            This function constructs a interface id to address mapping.
            Specifically, it mapps the interface ids to the addresses of routers
            who has the interface ID. The interface ids that each router has is
            defined in the .conf file and the topology file. 

            This mapping is required when forwarding the PCBs to the downstream
            ADs. After the propagate() function adds the markings it puts the
            interface ID to the special opaque field. Then it sets the address as
            the address of the router who owns that interface ID. 
        */
        void constructIfid2AddrMap(); 
        /**
            @brief UNUSED
        */
        void printPaths();
        /**
            @brief UNUSED 
        */
        void print(uint8_t* path, int hops); 
        /**
            @brief Sends the packet to the given port number.
            @param uint8_t* data The packet data that will be sent. 
            @param uint16_t dataLength The length of the data.
            @param int port click port number
			@param int fwd_type packet forwarding type: to a host (TO_SERVER) or to a router (TO_ROUTER) 

            This is a wrapper function that contains the click router routine that
            creates a new click packet with the given data and send the data to
            the given port.  
        */
        void sendPacket(uint8_t* data, uint16_t dataLength, string dest);
        /**
            @brief Gets the certificate file.
            @param uint8_t* fn The buffer that will store the path of the
            certificate file. 
            @param uint64_t target The AID of the owner of the certificate. 
        
            This function returns the path of the certificate file that is owned
            by the 'target'. 
            
            @not The path that this function passes to the param 'fn' is hard
            coded. MUST BE MODIFIED.  

        */
        void getCertFile(uint8_t* fn, uint64_t target);

        /**
            @brief Loads the private key for signature generation. 
        
            This function loads the private key from the private key file that is
            located in the file system. The path to the private key file is stored
            in m_csPrvKeyFile field and is defined in the .conf file. The field
            m_csPrvKeyFile is declared in the initVariable() function.
            
            When the private key is successfully loaded the _CryptoIsReady field
            is set to 'true'. Otherwise the value will set to false, indicating
            that the cyrpto operation is currently unavailable.    
        */
        void loadPrivateKey();
       
        /**
            @brief Verify unverified PCB.

            This function iterates through the unchecked PCB queue and verifies
            the PCBs. If the check fails in this function, the PCB is discarded
            desregarding the reason.   
        */
        void recheckPcb();
        
        /**
            @brief Get Opaque Field Generation Key.
            @param uint32_t timestamp The timestamp that will decide the Opaque
            field generation key. 
            @param aes_context &actx The aes structure that will store the key. 
       
            This function gets the opaque field generation key depending on the
            timestamp. The Beacon Server keeps two different keys. One is current
            and one is old. Along with the keys, the server keeps the time when
            each key is generated. When the givne timestamp is later than the
            timestamp of the current key, the current key is returned. Else, the
            old key is returned.   
             
        */
        
#ifdef ENABLE_AESNI
		bool getOfgKey(uint32_t timestamp, keystruct &ks);
#else
		bool getOfgKey(uint32_t timestamp, aes_context &actx);
#endif
        
        /**
            @brief Removes signature from PCB
            @param uint8_t* inPacket The buffer that contains the packet. 
            @param uint8_t* outPacket The buffer that will store the output. 

            This function removes all the signatures of the given PCB. The purpose
            of this function is to reduce the size of the packet when sending it
            to the hosts or path server. In those two cases, the signatures are
            not necessary and the size of the signature is significantly big.  
        */

        uint16_t removeSignature(uint8_t* inPacket, uint8_t* outPacket);
        
        /**
            @brief send request packet to Certificate Server
            @param uint8_t* packet The buffer that contains the PCB. 
        
            This function sends out Certificate request packets to the local
            Certificate server.  This function is triggered whenever the server
            decides there are more certificates needed to verify PCBs.
            
            Most of the cases are from verifyPcb() function where the verification
            fails due to the lack of necessary certificates.   
        */
        void requestForCert(uint8_t* packet);
		void clearCertificateMap();
		void clearAesKeyMap();
        /**
            @brief Chooses path to register
            @param multimap<uint32_t, pcb>::reverse_iterator &itr The iterator of
            the beacon table. 
            @return Returns true when a path is found. False when there is no path
            to register. 

            This function iterates through the beacon_table and selects the PCBs
            to retister. Whenever a PCB that contains the path that satisfies the
            path selection policy, the function stops the iteration and returns. 

            The iterator value that is passed into the function remains at the
            position where the selected path is located. If the path is not found
            it will remain at the beacon_table.end() position. 
        */
		bool getPathToRegister(std::multimap<uint32_t, pcb>::reverse_iterator &itr);

		//SL:
        /**
            @brief Initializes OfgKey
            @return Returns true if the key is succesfully initialized. False on
            any error. 

            This function initializes the opaque field generation key. The Beacon
            Server maintains a timestamp along with the OFG key. The timestamp is
            generated inside this function and the time is decreased by 300
            seconds. This allows the PCBs that are 5 minutes old to use the
            current key. 
            
            The new key is generated by using the Master Key defined in the .conf
            file.  
        */
		bool initOfgKey();
        /**
            @brief Updates OfgKey to a new value

            This function updates the opaque field generation key to a new key
            whenever there is a new key available at the certificate server. When
            the certificate server generates a new key, beacon server updates its
            current opaque field generation key. However, the old key is not
            discared but saved in the m_prevOfgKey field with the timestamp
            associated with it.  
        */
		bool updateOfgKey();

		//HC: initialize selection policy
    	void initSelectionPolicy();

		//SLN:
        /**
            @brief Requests the certificate server for ROT.
            
            This function sends out ROT request packet to the local certificate
            server.  
        */
		void requestROT();
		void requestROT(uint8_t * packet, uint32_t version);
        /**
            @brief Main Routine of the PCB process
            @param uint8_t* packet The buffer that contains the PCB. 
            @param uint16_t packetLength

            This function is the main routine that all the PCBs go through. It is
            the wrapper function of PCB verification, Addtion of new Information
            to the PCB, and propagation.  
        
        */
		void processPCB(uint8_t * packet, uint16_t packetLength);
        /**
            @brief Saves the Certificate to the local file system. 
            @param uint8_t* packet The packet that contains the certificate
            @param uint16_t The length of the packet. 

            This function saves the certificate file in the Certificate Reply
            packet (param 1) to the local file system. It creates a new file for
            the certificate inside the TD directory and writes the certificate
            information to that new file. 
            
            @note The path of the certificate file is hard coded. 
            @note The fragmentation of the file might be necessary.
        */
		void saveCertificate(uint8_t * packet, uint16_t packetLength);
		void updateIfidMap(uint8_t * packet);
        /**
            @brief Saves the ROT to the local file system. 
            @param uint8_t* packet The packet that contains the ROT.
            @param uint16_t The length of the packet. 

            This function saves the ROT file in the ROT Reply
            packet (param 1) to the local file system. It creates a new file for
            the ROT inside the TD directory and writes the ROT 
            information to that new file. 
            
            @note The path of the ROT file is hard coded. 
            @note The fragmentation of the file might be necessary.
        */
		void saveROT(uint8_t * packet, uint16_t packetLength);
		void sendAIDReply(uint8_t * packet, uint16_t packetLength);
		void initializeOutputPort();

    private:

        Timer _timer;
        Task  _task;        

        String m_AD;
        String m_HID;

        /** AD AID of pcb server */
        uint64_t m_uAdAid;                  //AD AID of pcb server
        /** TDID of the pcb server */
        uint16_t m_uTdAid;                    //TDID
        /** AID of the pcb server */
        uint64_t m_uAid;                     //AID of pcb server
        /** Address of the pcb server */
		HostAddr m_Addr;

        /*Temporary Values*/
        bool m_bROTInitiated;
        bool m_bROTRequested;
		bool _CryptoIsReady;
		bool _AIDIsRegister;
        
        /** Certificate File name */
        char m_csCertFile[MAX_FILE_LEN];
        /** Private Key File name */
        char m_csPrvKeyFile[MAX_FILE_LEN];
        /** Log file name */
        char m_csLogFile[MAX_FILE_LEN];

        /** Config File */
        String m_sConfigFile;
        /** Topology File */
        String m_sTopologyFile;
        /** ROT File */
        String m_sROTFile;
    
        /** UNUSED */
        int m_iFval;
        /** Number of path that can be registered */
        int m_iKval;
        /** UNUSED */
        int m_iPval;
        /** Path Register Interval */
        int m_iRegTime;
        /** Beacon Propagation Interval */
        int m_iPropTime;
        /** Queue Reset Interval */
        int m_iResetTime;
        int m_iScheduleTime;
        /** PCB Recheck Time Interval */
        int m_iRecheckTime;
        /** Logging Level */
        int m_iLogLevel;
        /** Registeration Flag, 1 if this AD is registering paths */
        int m_iIsRegister;  
        /** Number of registered path */
        int m_iNumRegisteredPath;
        /** Beacon Table Size */
        int m_iBeaconTableSize;
        /** Signing Private Key */ 
		rsa_context PriKey;
	    /** ROT */
        ROT m_cROT;
        /** Master Secret Key */
        uint8_t m_uMkey[MASTER_SECRET_KEY_SIZE];
        /** Timestamp of most recent propagation*/
        time_t m_lastPropTime;
        /** Timestamp of most recent registeration */
        time_t m_lastRegTime;
        /** Timestamp of most recent Recheck */
        time_t m_lastRecheckTime;
        /** Opaque Field Generation Key that is currently used */
		ofgKey m_currOfgKey;
        /** Past Ofg Key */
		ofgKey m_prevOfgKey;
		/** Interface mapping between this AD and the neighbors (myinterface,
         * neighbor interface)*/
        std::map<uint16_t, uint16_t> ifid_map;
        /** Interface - Address mapping between current AD's interfaces and the
         * neighbors that are connected to that interface */
        std::map<uint16_t, HostAddr> ifid2addr;
        
        /** Opaque field generation key table */
        std::map<uint32_t, ofgKey> key_table;
        /** Opaque field generation key table in aes_context */
        std::map<uint32_t, aes_context *> m_OfgAesCtx;
		//SL: use a pointer instead of an object if element is big
		//yet seems to be okay if not frequently accessed
		//for infrequently accessed objects, memory management can cause more overhead.
        /** Beacon Table. Maps the timestamp and the PCB */
        std::multimap<uint32_t, pcb> beacon_table;
        /** Unverified PCB list. Maps the timestamp and the PCB */
        std::multimap<uint32_t, pcb> unverified_pcbs;
        /** Table that contains the registered K paths */
        std::multimap<uint32_t, pcb> k_paths;
		//////////////////////////////////////////////////////////
        /** List of certificate requests that are sent 
            Maps the Owner of the certificate and the requested version. 
        */
        std::map<uint64_t, int> certRequest;
        /**
            List of internal servers.
            Indexed by the server type.
        */
        std::multimap<int, ServerElem> m_servers;
        /**
            List of internal routers.
            Indexed by the connected neighbor type.
        */
        std::multimap<int, RouterElem> m_routers;
        /**
            List of internal gateways.
        */
        std::multimap<int, GatewayElem> m_gateways;    
        /**
            List of certificates. 
            Maps between the owner AID and certificates in x509_cert type 
        */
		std::map<uint64_t, x509_cert*> m_certMap;
        SCIONPrint* scionPrinter;
       
        /** 
		    SCION Encapsulator for IP tunneling
        */
		SCIONIPEncap * m_pIPEncap;
        /**
            Address type and address of each interface.
            Each interface is indexed by the assigned port #
            struct portInfo is defined in packetheader.hh
        */
		vector<portInfo> m_vPortInfo;	
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


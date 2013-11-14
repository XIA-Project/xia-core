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
#include "scionipencap.hh"
#include "scionipdecap.hh"

// Tenma, added for AESNI
#ifdef ENABLE_AESNI
#include <intel_aesni/iaesni.h>
#endif

CLICK_DECLS

/**
    @brief SCION Edge router class.
    
    
    //@note This is not an implementation of any internal routers inside ADs. We
    //assume that internal routing is done by other intra AD routing protocol other
    //than SCION. 
    
    This class implements the SCION edge router (simply SCION router) using click router element. 
    
    SCION router performs (1) verification of packet-carried forwarding information (i.e., opaque field) 
	and (2) packet forwarding. We note that SCION router forwards packets using the forwarding
	information in the opaque field without performing any table (memory) lookup, which enables
	scalable and fast packet forwarding at routers.

	0. Bootstrapping
		During the router's initialization, SCION router discovers which router in its neighbor
		AD it connects to (i.e., the interface id of the connected router). One can statically 
		configure this information, yet in this case, the router's configuration should be
		changed whenever its neighbor change the id of interface card (e.g., network reconfiguration).
		To avoid this inconvenient dependency, a router requests an interface id to its neighbor,
		and the router, once it is replied with the corresponding interface id, forward the reply
		to Beacon Server. Note that Beacon Server has to keep the running state of each interface
		of its AD to schedule PCBs (i.e., path announcement). Furthermore, for peering link marking,
		neighbors' interface ids must be added to PCB so that endpoint hosts can discover a peering link
		when joining up- and down-paths. More details are described in the SCION design document.
		Note: Router's interface section in the topology file only contains the connected AD 
		(without the interface id).

    1. Verification of Opaque Field (Non-PCB packets). \n
        Each SCION router verifies the current opaque field in the SCION header
		to check if the packet contains valid path credentials.
		Opaque field consists of ingress and egress interfaces and the MAC created
		by Beacon Server with AD's secret key.
        For this purpose, SCION router verifies (1) whether the packet arrives 
		at the correct interface specified in the opaque field and (2) whether the MAC is
		correct. If the verification is successful, SCION router forwards the packet
		to the egress interface in the opaque field. Otherwise, the packet will be dropped by
        the router. Null egress interface (i.e., egress_if == 0) indicates that 
		the packet's destination is in the AD, so the router
		would forward the packet to the destination host (see below). \n
		Note: PCB packets do not contain the current AD's opaque field, hence opaque field 
		verification is irrelevant. SCION router, if it received a PCB from the upstream AD, 
		would mark the ingress interface id in the header (to help Beacon Server to create
		opaque field; see the PCB format for detail) and forward the PCB to Beacon Server.
		

    2. Fowarding Data/Control Packets. 
        SCION Router handles control and data packets differently. However, some 
		control packets (e.g., path registration to TDC -- which can be considered as a
		data packet; however, we assign a special type to path registration, hence classify it
		a control packet. This may be changed later once we have ANYCAST type to access special 
		servers, e.g., beacon, certificate, and path servers) are handled in the same manner
		as data packets. SCION router forwards a data packet to the egress interface specified in
		its opaque field after the current opaque field pointer. The opaque field pointer is 
		increased by 1 in case for normal forwarding, 2 for crossover forwarding, and 3 for
		peering link forwarding (see SCION documentation for detail). We note that if a packet is
		traversing toward TDC (i.e., in an uppath), the ingress and egress interfaces are interpreted
		on the other way by the router (since PCB always follows downpath). 
		
		Note: the ingress router on the crossover AS (where uppath ends) sets the current opaque
		field pointer to the downpath timestamp to inform the egress router of the downpath
		of the packet being forwarded through a crossover AS.
       	
		SCION router forwards control packets (such as PCB, ROT_REQ) to the corresponding servers.
		For example, PCB would be sent to Beacon Server, and ROT request be sent to Certificate Server.
		Those control packets do not have destination address, yet have the field reserved so that 
		SCION routers fill the address with that of the responsible server.
*/
class SCIONRouter : public Element { 
    public :
        /**
            @brief SCION Router Constructor

            @note Constructor registers the element timer, whose handler is called by
			a click scheduling mechanism, via :_timer(this). 
            More detailed information is found in the click->Timer element class documentation.
        */
        SCIONRouter():_timer(this){
            m_iCore = 0;
            m_iLogLevel = 300;
        };
        ~SCIONRouter()
        {
            delete scionPrinter;
			delete [] m_pIPEncap;
        };
       
        //const char *flow_code()  const {return "x/x";}
        const char *class_name() const {return "SCIONRouter";}
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
        /**
            @brief Send a packet to the specified port
            @param uint8_t* data The pointer to the packet in the memory.
            @param uint16_t dataLength The length of the packet. 
            @param int port The (Click) port number to which the packet will be written.
            
            This function creates a packet in the format that can be forwarded 
			by the specified output port. For example, the output port is configured
			as a native Click port (i.e., no address is assigned to the port), then
			the packet is encapsulated with Click packet. If the output port is assigned
			an IP address, IP encapsulation is performed before the Click encapsulation.
			Currently only IPv4 is supported, yet other address (e.g., IPv6, AIP, MPLS) can 
			be supported later. 

        */
        void sendPacket(uint8_t* data, uint16_t dataLength, int port, int type=0);
        
        int configure(Vector<String> &, ErrorHandler *);

        int initialize(ErrorHandler* errh);

        void run_timer(Timer *); 
        
        void push(int, Packet*); 

        /**
            @brief Return the current opaque field generating key.
            @param uint32_t timestamp The timestamp with which the opaque field is generated by Beacon Server.
			Note that if the expiration time of the key is 24 hours (i.e.,
			the corresponding path can be used for 24 hours), two opaque field generation keys
			are always valid for a given time. 
            @param aes_context &actx AES context structure that contains the AES key information 
			(e.g., key, round keys). 
            @return SCION_SUCCESS when the key is successfully loaded.
            SCION_FAILURE when the key cannot be loaded for some reason: we use the polar error code
			without modification.  

            The router keeps two opaque field generation keys: current and previous keys   
			along with the key issue times.
            If the input timestamp to the function is older 
            than the issue time of the current key,
            the AES context prepared with the previous key will be returned. 
			Otherwise, the AES context prepared with the current key will be returned.
        
        */
#ifdef ENABLE_AESNI
		bool getOfgKey(uint32_t timestamp, keystruct &ks);
#else
		bool getOfgKey(uint32_t timestamp, aes_context &actx);
#endif
       
        /**
            @brief Returns the interface id (IFID) that corresponds to a given 
			(Click's) port number. 
            @param int port The port number whose IFID is looked for.
            @return The IFID that maps to the given port number. 
            
            The SCION Routers keep the mapping between IFID (interface id) 
            and the Click's port number. IFID is a unique identifier assigned to
			each ingress/egress interface by AD's administrator. IFID is used for PCB
			generation/propagation. Click's port number is enumerated from 0 (which is
			the port connected to SCIONSwitch) up to the number of router's interfaces.
			This mapping is created during the initialization phase.
        */ 
        uint16_t getIFID(int port);
        
        /**
            @brief Initializes information in the bootstrapping process. 
            
            Initializes variables that are required for the SCION Router to run.
            Not ALL the fields in the SCION Router are initiated in this function
            but majority of the fields are initiated. 

            This function initializes three main types of fields. 
            
            1. Configuration File information
                The Config() is called and the router's configuration information
                is read from the configuration file.                 
				
            2. Topology File information
                The topology parser is created and the topology information is
                parsed into the fields. This is done by calling parseTology().
                Please refer to the spec of this function for more detail.
                
            3. Interface ID information. 
                This information mapps the Interface IDs to the SCION Addresses of
                the routers. This mapping is required when the routers send out
                packet to the routers who owns the egress interface ID.         
        
        */
        void initVariables();

        /**
            @brief Maps the click port numbers to the assigned Interface IDs. 

            Refer to getIFID() for details of IFID and port numbers. 

            This function creates a bidirectional mappings between the click port numbers and
            SCION Interface IDs. Port-to-IFID and IFID-to-port mappings are stored in
            port2ifid map and ifid2port respectively.
            
            This function must be called after the configuration file is parsed
            and the corresponding fields are initialized. This is because
			the ifid_set used in this function is created
            during the configuration file being parsed.
            
            @note If the number of IFIDs exceeds the number of click ports (defined
            in .click file), the system will throw an exception. 
			//This is a
            //click specification and will be patched so that the system exits with
            //a warning instead of crashing.  
        */
        void mapIFID2Port();

        /**
            @brief Parses topology file from the file system

            This function loads and parses the Topology information. The
            TopoParser object is created using the topology file that is specified
            in the configuration file. 

            When the topology file is loaded successfully, parseServers() and
            parseRouters() functions are called. These two function initializes
            the m_servers and m_routers that contain the information about 
			the servers and routers respectively. 

            @note This function must be called only after the configuration file
            is parsed and the corresponding fields are filled. 
        */
        void parseTopology();

        //SL: new functions
        /**
            @brief Sends the packet to the egress interface. 
            @param int port
            @param uint8_t* pkt
            @return int

            This function sends the 'pkt' to the port that matches with the
			interface ID in the router. 
        */
		int writeToEgressInterface(int port, uint8_t * pkt);

        /**
            @brief Main function that performs data packet forwarding.  
            @param int port The click port number to which data packet is forwarded. 
            @param uint8_t* pkt Packet to be verified and forwarded. 
            @return SCION_SUCCESS when the Opaque Field verification is passed and
            the packet is forwarded successfully. Otherwise, SCION_FAILURE is returned.

            This function performs opaque field verification and packet forwarding. 
			Opaque filed verification includes (1) ingress interface verification and
			(2) MAC verification. Packet forwarding differs based on how the path is 
			constructed by the source and based on the router's location on the path.
			More details on opaque field verification and forwarding are explained 
			in the design document. 
        */
		int forwardDataPacket(int port, uint8_t * pkt);

        /**
            @brief Handler of all the control packets. 
            @param int port The click port to which the packet is forwarded. 
            @param uint8_t* pkt The control packet. 
            @return SCION_SUCCESS when the packet is successfully forwarded.
            SCION Error code when any sort of failure occurs. 

            This function handles all the control plane packets such as PCB, path
            registration packets and etc. Please refer to the packetheader.hh for
            more types of control packets.
            
            Each type of control plane packets should be handled in different ways
            as described in the Class Detailed Descriptions. 
        */
		int processControlPacket(int port, uint8_t * pkt);

        /**
            @brief Verifies the interface number with the physical port
            @param int port The port number to be verified. 
            @param uint8_t* packet Packet that came into the router. 
            @param uint8_t dir Direction value that represents the up/down path of
            the packet. 
            @return SCION_SUCCESS when the Interface ID matches the click port
            number. SCION_FAILURE otherwise.  
           
            SCION Edge Router, other than verifying
            the MAC value, is to verify if the packet actually came from the
            Interface specified in the opaque field. 
            
            On the up path, the packet must come from the port number that matches
            with the egress interface of the opaque field. On the other hand, in case
            of down path, the path must come from the port number that matches with the
            ingress interface id in the opaque field.  
            
            The IFID and Port number mapping is done by mapIFID2Port() function.
            Please refer to mapIFID2Port() and getIFID() for more information
            about IFID and Port numbers. 
        */
        int verifyInterface(int port, uint8_t* packet, uint8_t dir);


        /**
            @brief Maps the Interface IDs to the SCION Addresses of the Routers. 
            
            This function constructs a map between a router's interface ID and
			the corresponding address. The SCION Edge Routers should
            forward the Data packets directly to the egress Edge Router when the
            MAC verification of a packet passes. To forward the packet to the router
			that owns the interface ID, each router holds the IFID-to-Addr map and 
			use it if necessary. For example, if intra-AD packet forwarding is 
			performed using IP encapsulation, an ingress router needs to encapsulate
			a packet with the egress router's IP address. 

            @note Address and Interface information based on that from the
            topology file. Please refer to the TopoParser() class of how this
            information is parsed.  
        */
		void constructIfid2AddrMap();

        /**
            @brief Initializes the output port for the IP
            Prepares IP header for IP encapsulation if the port is assigned
            as IP address
        */
		void initializeOutputPort();	
        
        //SL:
		//verify opaque field (not crossover point)
		//this should handle uppath and downpath OFs differently based on the current Flag bit
        /**
            @brief Verifies opaque field for Data Packets 
            @param int port The port at the packet arrived.  
            @param uint8_t* pkt The packet to be verified. 
            @return SCION_SUCCESS when the opaque filed verification passes.
            SCION_FAILURE otherwise. 

            This function verifies the opaque field of a packet. This
            function first checks whether the packet the packet came from external AD
			by looking at the 'port' value. (Note: internal port is assigned a port number 0) 
			If the packet came from the external AD, the
            verifyInterface() function is called to check if the packet carries a correct
            interface information in the opaque field. 
            
            This function considers all forwarding cases (described in the
            class detail): up path, down path, crossover and peering. This can
            be determined by looking at the flag field in the SCION common header, and info field
			in the opaque field. 
            
            After verifying the incoming interface, the
            router verifies MAC in the opaque field using its own secret key.
            
        */
		int verifyOF(int port, uint8_t* pkt); //verify opaque field (not crossover point)

		//SL:
        /**
            @brief Initializes OFG (Opaque Field Generation)  Key. 
            @return SCION_SUCCESS when the OFG Key is successfully loaded into
            m_currOfgKey. SCION_FAILURE when failure.
             
            This function initializes the Opaque Field Generation Key using the
            current time as timestamp. The initial key is set as m_uMKey, and the
            aes_context structure of the currentKey structure is filled by
            aes_setKey_enc() function from the polarssl library. 

            This function is called during the bootstrapping step. 
			
            @note Please refer to the polarssl library documentation about this
            function.
        */
		bool initOfgKey();
        /**
            @brief Updates the OFG Key with a new value and stores the current
            value in the m_prevOfgKey structure. 
            @return SCION_SUCCESS when the key is successfully generated and
            loaded into the aes_context structures.
            
            This function is called when the certificate server updates OFG key and 
			sends the key to the border router.
            
            TODO: This function is not fully implemented yet; certificate server's key
			update needs to be implemented.

        */
		bool updateOfgKey();

		//SLN:
        /**
            @brief Subroutine that performs data packet forwarding when the packet is either on uppath or on downpath.  
			@param int type Packet type
            @param int port The click port number to which data packet is forwarded. 
            @param uint8_t* pkt Packet to be verified and forwarded. 
			@param uppath int uppath Flag indicating whether the packet is following uppath (1) or downpath (0).
            @return SCION_SUCCESS when the Opaque Field verification is passed and
            the packet is forwarded successfully. Otherwise, SCION_FAILURE is returned.
        */
		int normalForward(uint8_t type, int port, uint8_t * packet, uint8_t uppath);
        
		/**
            @brief Subroutine that performs data packet forwarding when the packet is at the crossover AD.  
			@param int type Packet type
			@param int info info field in the opaque field (which specifies the crossover type; e.g., TDC crossover
			shortcut, peering link)
            @param int port The click port number to which data packet is forwarded. 
            @param uint8_t* pkt Packet to be verified and forwarded. 
			@param uppath uint8_t uppath Flag indicating whether the packet is following uppath (1) or downpath (0).
            @return SCION_SUCCESS when the Opaque Field verification is passed and
            the packet is forwarded successfully. Otherwise, SCION_FAILURE is returned.
        */
		int crossoverForward(uint8_t type, uint8_t info, int port, uint8_t * packet, uint8_t uppath);

    private:
      
        Timer _timer;    
        /** 
            mapping between neighboring ifids
			used for peering link
        */ 
        map<uint16_t, uint16_t> ifid_map;	 
        /** 
            set of interface ids that belong to this router
        */ 
        vector<uint16_t> ifid_set;		
        /** 
            address type and address of each interface
			each interface is indexed by the assigned port #
        */ 
		vector<portInfo *> m_vPortInfo;	         
        /** 
          port to interface id mapping
          redefined as a vector to speedup
        */ 
		vector<uint16_t> v_port2ifid;	        
        /** 
            Mapping between the click port and the interface id 
        */ 
        map<int, uint16_t> port2ifid;	
        /** 
          ifid to port mapping to determine the egress port 
          of a packet quickly
        */ 
        map<uint16_t,int> ifid2port;
        /** 
            ifid to address mapping; used for IPEncap
        */ 
		map<uint16_t, HostAddr> ifid2addr;	
        
        /** Number of Interfaces */
        int m_iNumIFID; 
        /** Configuration file */
        String m_sConfigFile;
        /** Topology File */
        String m_sTopologyFile;
        /** Log file name */
        char m_csLogFile[MAX_FILE_LEN];
        
        /** 
          contains server list based on the type;
          indexed by type; i.e., BS, CS, PS
        */ 
        std::multimap<int, ServerElem> m_servers;	
        /** 
          contains router list
          indexed by connected neighbor type; 
          i.e., provider, customer, peer
        */ 
        std::multimap<int, RouterElem> m_routers;	
        /** 
            List of gateways.
            indexted by connected neighbor type 
        */ 
        std::multimap<int, GatewayElem> m_gateways;         
		
		/** 
            Current OFG Key 
        */ 
		ofgKey m_currOfgKey;	//current OFG key
        /** 
            Previous OFG Key
        */ 
		ofgKey m_prevOfgKey;	//previous OFG key
        /** 
            Master key that is used when generating the OFG keys.
        */
        uint8_t m_uMkey[16];	//master key
		/**
          (timestamp, key) table
          outdated -- we decided to use two keys
          leave this for a while for our record
        */ 
		std::map<uint32_t, ofgKey> key_table;	
        /** 
          (timestamp, key) table
          outdated -- we decided to use two keys
          leave this for a while for our record
        */ 
        std::map<uint32_t, aes_context*> m_OfgAesCtx;	//same as above
        /** 
            AID of the router. 
        */ 
        uint64_t m_uAid;		//router's AID
        /** 
            Address of the router.
        */ 
		HostAddr m_Addr;		//router's Addrress (not each interface's)
        /** 
            ADID of the AD where this router belongs.
        */ 
        uint64_t m_uAdAid;		//AD where this router belongs
        /** 
            TDID of the TD where the AD of this router belongs.  
        */ 
        uint16_t m_uTdAid;		//TD where this router belongs
        /** 
            Core Flag. If 1 then this router is a core router.
        */ 
        int m_iCore;			//set if this router belongs to TDC AD
        /** 
            Logging Level.
        */ 
        int m_iLogLevel;
        /** 
            SCION Printer element for logging.
        */ 
        SCIONPrint* scionPrinter;

		//For IP tunneling
        /** 
          IP header that corresponds to each port (i.e., ifid)
          this should be initialized while reading configuration
          once initialized, ip encap would be performed by computing 
          IP checksum incrementally
        */ 
		SCIONIPEncap *m_pIPEncap;	
};

CLICK_ENDDECLS


#endif

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

#ifndef SCIONPATHSERVER_HEADER_HH_
#define SCIONPATHSERVER_HEADER_HH_

#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/timer.hh>
#include <click/notifier.hh>
#include <click/task.hh>
#include <map>
#include <vector>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <tr1/unordered_map>

/*include here*/
#include "define.hh"
#include "packetheader.hh"
#include "scionbeacon.hh"
#include "topo_parser.hh"
#include "config.hh"
#include "scionprint.hh"
#include "scionipencap.hh"

/**
    @brief UP path struct.
    Parsed up path is stored inside this structure. 
*/
struct upPath
{
    /** number of hops in the path */
    uint16_t hops;
    /** number of peers in the path */
    uint16_t numPeers;
    /** List of ADs in the path */
    std::vector<uint64_t> path;
    /** List of Peers in the path */
    std::multimap<uint64_t, uint64_t> peers;
    /** RAW path in bytes */
    uint8_t* msg;
};

/**
    @brief DOWN path struct. 
    Parsed DOWN path is stored inside this structure
*/
struct downPath{
    /** number of hops in the path */
    uint16_t hops;
    /** number of peers in the path */
    uint16_t numPeers;
    /** List of ADs in the path */
    std::vector<uint64_t> path;
    /** List of Peers in the path */
    std::multimap<uint64_t, uint64_t> peers;
    /** Destination AD (owner) of this down path */
    uint64_t destAID;
    /** Raw path in bytes */
    uint8_t* msg;
};

/**
    @brief end-to-end path (full path) structure. 
    This structure stores the end to end path. 
*/
struct end2endPath{
    /** True if this path takes shortcut */
    bool isShortcut;
    /** Number of hops in the path */
    uint16_t hops;
    /** Number of peers in the path */
    uint16_t numPeers;
    /** List of ADs in the path */
    std::vector<uint64_t>path;
    /** List of peers in the path. Mapped with their ADID*/
    std::multimap<uint64_t, uint64_t> peers;
    /** Destination AD of this end to end path */
    uint64_t destAID;
    /** Raw path in bytes */
    uint8_t* msg;
};

class UPQueue
{

    // Forward declaration of Element struct.
    //struct Element;

public:

    /**
     * PathQueue (constructor)
     * -------------------------------------------------------------------------
     * Creates an empty PathQueue object with size limit maxSize.
     *
     * INPUTS/PRECONDITIONS:
     *   None.
     *
     * OUTPUTS/POSTCONDITIONS:
     *   Creates a new PathQueue object with the specified size limit.
     */
    UPQueue(size_t maxSize);

    // Copy constructor.
    //UPQueue(const UPQueue &orig);

    // Assignment operator.
	//SLP: blocked...
    //UPQueue& operator=(const UPQueue &rhs);

    // Destructor.
    ~UPQueue();

    /**
     * isEmpty (constant member function)
     * -------------------------------------------------------------------------
     * Checks whether the queue is empty or not.
     *
     * INPUTS/PRECONDITIONS:
     *   None.
     *
     * OUTPUTS/POSTCONDITIONS:
     *   Returns true is the queue is empty and false otherwise.
     */
    bool isEmpty() const;

    /**
     * size (constant member function)
     * -------------------------------------------------------------------------
     * Determines the current size of the queue.
     *
     * INPUTS/PRECONDITIONS:
     *   None.
     *
     * OUTPUTS/POSTCONDITIONS:
     *   Returns the size of the queue.
     */
    size_t getSize() const;

    /**
     * enqueue (member function)
     * -------------------------------------------------------------------------
     * Adds a path to the end of the queue.
     *
     * INPUTS/PRECONDITIONS:
     *   The up-path to add to the queue.
     *
     * OUTPUTS/POSTCONDITIONS:
     *   The new path is added to the end of the queue.
     */
    void enqueue(upPath* path);

    /**
     * dequeue (member function)
     * -------------------------------------------------------------------------
     * Deletes the path at the front of the queue.
     *
     * INPUTS/PRECONDITIONS:
     *   None.
     *
     * OUTPUTS/POSTCONDITIONS:
     *   The up-path at the front of the queue is dequeued.
     */
    void dequeue();

    /**
     * headPath (member function)
     * -------------------------------------------------------------------------
     * Returns the path at the front of the queue.
     *
     * INPUTS/PRECONDITIONS:
     *   None.
     *
     * OUTPUTS/POSTCONDITIONS:
     *   Returns the first path in the queue. The path remains on the queue.
     */
    upPath* headPath();

    /**
     * tailPath (member function)
     * -------------------------------------------------------------------------
     * Returns the path at the end of the queue.
     *
     * INPUTS/PRECONDITIONS:
     *   None.
     *
     * OUTPUTS/POSTCONDITIONS:
     *   Returns the last path in the queue. The path remains on the queue.
     */
    upPath* tailPath();

private:

    size_t head;
    size_t tail;
    size_t size;
    size_t maxSize;

    upPath** paths;
};

struct PathHash
{
    size_t operator()(const scionHash &h) const;
};

enum pathCriteria {DISTANCE,
                   FRESHNESS,
                   UNASSIGNED_3,
                   UNASSIGNED_4,
                   UNASSIGNED_5,
                   UNASSIGNED_6,
                   UNASSIGNED_7,
                   UNASSIGNED_8};

CLICK_DECLS

/**
    @brief SCION Path Server element class.  

    This class is the implementation of the SCION Path Server. SCION Path
    Server has two types, the TDC Path Server and Non-TDC Path Server. This
    class is the implementation for the Non-TDC Path Server. The reference for
    the TDC Path Server can be found in scionbeaconserver_core.hh. 

    The Non-TDC Path Server (Path Server) is responsible of three things. 

    1. Parsing and Storing up-paths. 
       
    When the beacon server sends the selected up paths to the path server, path
    server parses the path information from the received packets and store the
    paths into the memory. The format of the received paths are in the similar
    format as the PCB except for that it does not include any signatures. 
    Please refer to the beaconserver.hh for more details on how the UP_PATH
    packets are sent to the path server. 

    Whenever new paths arrive, the path server stores them until the maximum path
    table size is reached. Then, the path server removes the paths from the oldest
    to store the new receiving paths.
     
    2. Parsing and Storing down-paths. 
    
    Similarly, when the down path packet comes from the TDC path server, the path
    server parses and stores the down path into the memory. The format of the down
    path is similar to the up path. Since it is the beacon server that registers
    the paths to the TDC path server, the format is the PCB without the
    signatures. 

    Whenever new down paths are received, the path server stores them until the
    maximum table size is reached. Then, the path server removes the paths from
    the oldest to store the new receiving paths. 

    3. Down path request and reply
    
    The path server is responsible of requesting the TDC path server for down
    paths for other ADs and handling the reply packets from the TDC path server.
    When a local host requests for a down path for another AD, by using the
    PATH_REQ_LOCAL, the path server immediately sends out the PATH_REQ to the TDC
    path server to fetch the down path. When the reply arrives for the requested
    path, the path server stores the path as some type of cache for future use,
    and reply the path back to the requester by using the PATH_REP_LOCAL. 

    4. Up path request and reply

    The path server also handles the up path requsts from the local hosts. The
    local hosts must have at least one up path and down path pair to communicate
    with other ADs. Thus the end host will request for the up path as well as the
    dwon path if necessary. However, when the up path is requested, the path
    server only searches for the local memory and selects paths from it and sends
    back the reply.  
*/
class SCIONPathServer : public Element { 
    public :
        SCIONPathServer():_timer(this), _task(this){};
        ~SCIONPathServer(){clearPaths(); delete scionPrinter;};
       
        const char *class_name() const {return "SCIONPathServer";}
        const char *port_count() const {return "-/-";}
        const char *processing() const {return PUSH;} // same as "h/h"

        int configure(Vector<String> &, ErrorHandler *);
        int initialize(ErrorHandler* errh);
        void run_timer(Timer *);

        void push(int port, Packet *p);
        void sendHello();

        /**
            @brief Parses down path information from the PCB 
            @param uint8_t* pkt The packet that contains the down path. 

            This function parses the down path information from the downloaded
            Path from the TDC Path server.
        */
        void parseDownPath(uint8_t* pkt);

        /**
            @brief Parses up path information from the PCB
            @param uint8_t* pkt The packet that contains the up path. 

            This function parses and stores the up path information from the PCB
            that was sent from the Beacon Server. 
        */
        void parseUpPath(uint8_t* pkt);

        void printDownPath();

        /**
            @brief Builds an UP path using the Path information. 
            @param uint8_t* pkt The buffer that contains the PCB. 
            @param uint8_t* output The buffer that will store the path. 
        
            This function builds an up-path by using the information of the given
            Path packet. The output path contains all the opaque fields of the path
            and the special opaque field that contains the timestamp value.
            
            The returned path can be directly put into a DATA packet to forward
            DATA packets.    
        */
        int buildPath(uint8_t* pkt, uint8_t* output);
        
        /**
            @brief Sends path request to the TDC Path Server
            @param uint64_t The target ID which is the owner of the down path. 
            @param HostAddr The host address of the requester
            @return Returns error code. 

            This function sends a path request packet to the TDC path server. The
            target Address is specified in the parameter as the HostAddr. 

            This function returns 0
        */
        int sendRequest(uint64_t, HostAddr);

        /**
            @brief Sends up path to the clients
            @param uint32_t pref The preference value from the requesting host. 

            This function sends the up path to the clients. The clients uses the
            up path to send data packet to other ADs. 

            Returns 0.
        */
        int sendUpPath(uint32_t pref=0);
        
        /**
            @brief Sends up path to the clients
            @param HostAddr &requestId The ID of the requsting end host. 
            @param uint32_t pref The preference of paths of the host. 

            This function sends up path to the client. But in this function the
            path chosen by the server is pased on the requestId and the pref. 
            
            Returns 0.
        */
        int sendUpPath(HostAddr &requestId, uint32_t pref=0);

        scionHash createHash(uint8_t* pkt);

        /**
            @brief Initializes information in the bootstrapping process. 
            
            Initializes variables that are required for the SCION Router to run.
            Not ALL the fields in the SCION Router are initiated in this function
            but majority of the fields are initiated. 

            This function initializes three main types of fields. 
            
            1. Configuration File information
                The Config() is called and the configuration file for the router
                is parsed. Then the necessary accessor functions are called in
                order to fill up the fields.
                
            2. Topology File information
                The topology parser is created and the topology information is
                parsed into the fields. This is done by calling parseTology().
                Please refer to the spec of this function for more detail.
                
            3. Interface ID information. 
                This information mapps the Interface IDs to the SCION Addresses of
                the routers. This mapping is required when the servers send out
                packet to the routers who owns the egress interface ID.         
        */

        /**
            @brief Sends the packet to the given port number.
            @param uint8_t* data The packet data that will be sent. 
            @param uint16_t dataLength The length of the data.
            @param int port click port number

            This is a wrapper function that contains the click router routine that
            creates a new click packet with the given data and send the data to
            the given port.  
        */
        //void sendPacket(uint8_t* packet, uint16_t packetLength, int port, int fwd_type=0);
        void sendPacket(uint8_t* data, uint16_t dataLength, string dest);


        /**
            @brief Parse Topology information using TopoParser. 

            This function parses the topology file using the TopoParser element.
            The path of the topology file is defined in the .conf file. The
            m_sTopologyFile value is set to this path in the initVariable()
            function. Only when a valid path is set, this function will extract
            the topology information from the TopoParser into the m_clients, m_servers and
            m_routers structures.
            
            @note This function must be called after all the internal variables
            are initialized by initVariable() function.    
        */
        void parseTopology();

		//SLP:
		void clearPaths();
    private:
        /** Log file name */
        char m_sLogFile[MAX_FILE_LEN];
        /** Log level */
        int m_iLogLevel;
        /** Path Queue Size */
        int m_iQueueSize;
        /** Number of returned up path */
        int m_iNumRetUP;
        /** AID of the path server */
        uint64_t m_uAid;
        /** Address of the path server */
		HostAddr m_Addr;
        /** ADID of the path server */
        uint64_t m_uAdAid;

        String m_AD;
        String m_HID;

        /** unsorted list of paths using hash value as the key */
        std::tr1::unordered_map<scionHash, UPQueue*, PathHash> upPaths;
        /** List of Down paths */
        std::multimap<uint16_t, downPath> downPaths; 
        /** 
            List of local clients 
        */
        std::map<int,ClientElem> m_clients;
        /** 
            List of local servers 
            indexted by server type 
        */
        std::multimap<int, Servers> m_servers;
        /** 
            List of routers 
            indexted by connected neighbor type 
        */
        std::multimap<int, RouterElem> m_routers;
        /** 
            List of local gateways 
        */
        std::multimap<int, GatewayElem> m_gateways;         
        /** Mapping between the interface IDs and the neighbor's ADs */
        std::map<uint16_t, HostAddr> ifid2addr;

		//SL:
        /** 
            List of pending Down path request 
            Maps the target AD ID and the address of the requesting host 
        */
		std::multimap<uint64_t, HostAddr> pendingDownpathReq;
        /**
            List of time values for the pending requests.
        */
		std::map<uint64_t, time_t> pendingDownpathReqTime;
       
        Timer _timer;
        Task _task;
        int m_iTval;
        /**
            Configuration File
        */
        String m_sConfigFile;
        /** Topology File */
        String m_sTopologyFile;
        /** SCION printer for logging */
        SCIONPrint* scionPrinter;

        /**
            SCION encap element for IP tunneling.
        */
		SCIONIPEncap * m_pIPEncap;
        /**
            Port information for IP tunneling that contains address type and
            address of each interface.
        */
		vector<portInfo> m_vPortInfo;	//address type and address of each interface
};

CLICK_ENDDECLS


#endif

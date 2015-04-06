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
#include "scionipencap.hh"

CLICK_DECLS

/**
    @brief Path struct used in path server core. 
    This struct is used in path server core to parse paths. The content of the
    server is the downpaths that are registered from the endpoint ADs. 
*/
struct path{
    /** Number of hops inside the path */
    uint16_t hops;
    /** Number of peering links in the path */
    uint16_t numPeers;
    /** List of ADs in the path */
    std::vector<uint64_t>path;
    /** List of peers in the path */
    std::multimap<uint64_t, uint64_t> peers;
    /** Length of the path in bytes */
    uint16_t pathLength;
    /** Destination ADID (the owner) */
    uint64_t destAID;
    /** TDID of the destination */
    uint16_t tdid;
    /** Timestamp */
    uint32_t timestamp;
    /** Raw path in bytes */
    uint8_t* msg;
};


/**
    @brief SCION Path Server Core element class.  

    This class is the implementation of the SCION Path Server Core. SCION Path
    Server has two types, the TDC Path Server and Non-TDC Path Server. This
    class is the implementation for the Non-TDC Path Server Core. The reference for
    the Non-TDC Path Server can be found in scionbeaconserver.hh. 

    The TDC-Path Server's functionality is much simpler than the Non-TDC Path
    server. It is responsible of only maintaining the registered paths of the all
    the end-point (or whichever AD that registers path).  

    1. Storing registered Down-Paths.
    When the path registration packet comes, the TDC-Path server parses the path
    inside the registeration packet and stores the information into its data
    structure. The pahts are stored in a path table and they are indexed with the
    destination (the owner of the path) AD. 
     

    2. Down path request and reply
    When the path request packet comes, the TDC-Path server searches through its
    path table and returns the paths to the requester. The TDC-Path server
    attempts to find the set of paths that matches the 'option'. But when there is
    no paths that matches the option, it returns the shortest paths to the
    requester. 
    
*/
class SCIONPathServerCore : public Element { 
    public :
        SCIONPathServerCore(): _timer(this), _task(this){};
        ~SCIONPathServerCore(){delete scionPrinter;};
       
        const char *class_name() const {return "SCIONPathServerCore";}
        const char *port_count() const {return "-/-";}
        const char *processing() const {return PUSH;} // same as "h/h"

        int configure(Vector<String> &, ErrorHandler *);
        int initialize(ErrorHandler* errh);
        void run_timer(Timer *);
        void push(int port, Packet *p);
        void sendHello();

        /**
            @brief Parse path from PCB
            @param uint8_t* pkt
            @return void

            Parses path from the paths received from the beacon server. The path
            information is stored in a table for later use. 
        
        */
        void parsePath(uint8_t* pkt);

        void printPaths();

        /**
            @brief Reverse the order of the opaque fields inside a path. 
            @param uint8_t* path
            @param uint8_t* output
            @param uint8_t hops
            @return void

            Reverse the order of the path inside the 'path' and outputs it into
            'output'. This is because the order of up path and down path is
            opposite. 
        */
        void reversePath(uint8_t* path, uint8_t* output, uint8_t hops);
        
        /**
            @brief Sends packet to the specified port
            @param uint8_t* packet
            @param uint16_t packetLength
            @param int port
            @param int fwd_type
            @return void

            This function sends out click packet that contains the data 'packet'
            and sends it to the port specified as 'port.'
        */
        //void sendPacket(uint8_t* packet, uint16_t packetLength, int port, int fwd_type=0);
        void sendPacket(uint8_t* data, uint16_t dataLength, string dest);
        /**
            @brief Parses Topology information from the filesystem. 
            @param void
            @return void

            This function parses the topology file from the filesystem and stores
            it in the memory. 
        */
        void parseTopology();
		
    private:
        Timer _timer;
        Task _task;
        /** AID of the path server core */
        uint64_t m_uAid;
        /** Address of path server core */
		HostAddr m_Addr;
        /** AD ID of the path server core */
        uint64_t m_uAdAid;

        String m_AD;
        String m_HID;

		//std::multimap<uint64_t, path> paths;
        /** list of paths, mapped with the owner of the path */
        std::multimap<uint64_t, std::multimap<uint32_t, path> > paths;
        /** Tval */
        int m_iTval;
        /** Log file name of the path server core */
        char m_sLogFile[MAX_FILE_LEN];
        /** Log level */
        int m_iLogLevel;
        /** Number of registered paths */
        int m_iNumRegisteredPath;
        /** Configuration file name */
        String m_sConfigFile;
        /** Topology file name */
        String m_sTopologyFile;
        /** SCION printer for logging */
        SCIONPrint* scionPrinter;
        /** Mapping between local ifid and the neighbor's ADID */
        std::map<uint16_t, HostAddr> ifid2addr;
        /** 
            List of local servers 
            indexted by server type 
        */
        std::multimap<int, Servers> m_servers;
        /** 
            List of local edge routers
            indexted by connected neighbor type 
        */
        std::multimap<int, RouterElem> m_routers;

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

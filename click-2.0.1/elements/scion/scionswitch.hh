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

/**
    @brief SCION Switch Class

    This class implements the SCION switch. SCION switch is a temporary element
    that connects all the SCION servers and Edge Routers. The switch is developed
    because there was internal AD routing protocols that could be used for
    forwarding SCION packets. SCION switch can viewed as a abstract routing
    protocol that will be later replaced with real routing protocol.
    
    SCION Switch is responsible for forwarding the SCION packet to the correct
    SCION servers and routers. The assumption here is that all the SCION related
    servers and routers are directly connected with the switch in each AD.
    
    Each SCION servers and routers sends packet to switch except for those packets
    go outside the AD. For the packets that go outside the AD, SCION Edge Routers
    directly sends the packets to the neighboring SCION Edge Routers. Please refer
    to the SCION Implementation Slides for diagrams and more information.   
*/
class SCIONSwitch : public Element { 
    public :
        /**
            @brief SCION Switch Constructor

            The constructor of SCION switch retruns an instance of SCION switch. 
        */
        SCIONSwitch(): _timer(this) {
            m_iNumClients =0;
            };
        ~SCIONSwitch(){delete scionPrinter;};
       
//        const char *flow_code()  const {return "x/x";}
        const char *class_name() const {return "SCIONSwitch";}
        const char *port_count() const {return "-/-";}

        /**
            @brief Main routine of the SCION Switch where it handles the incomming
            packets. 
            @param int port The click port number where the packet will be sent
            out to. 
            @param Packet* pkt Incomming packet from other SCION servers and
            routers. 
            @note The packets, port numbers, and the function name 'push' 
            are those of click routers and the functionalities are exactly 
            the same as those in click. Please refer to the click router 
            documentation for more details about click packets and click port numbers. 

            This function can be divided into two parts. Data packet forwarding
            and Control Packet forwarding.   
            
            1. Data packet forwarding
            
                The switch forwards the data packet by looking at the outgoing
                interface of the packet. The outgoing interface that is specified
                in the opaque field can be retrieved by calling the
                SCIONPacketHeader::getOutgoingInterface() function. Then switch
                uses this value to get the address of the outgoing edge router.
                When the address is found in the map, the switch forwards the
                packet to the router. If not the switch drops the packet printing
                an error message on the log file.
            
            2. Control Packet Forwarding
            
                The switch handles all the control packets destined to the local
                servers at the same time. These packets have the destination
                address as the destination servers and routers. The switch uses
                these address to forward the control packets. 
                
                For other control packets that are going out to other ADs, the
                switch uses the outgoing interface to determine the corresponding
                router address.   
        */
        void push(int, Packet*); 
        
        
        void run_timer(Timer *timer);
        
        const char *processing() const {return PUSH;}

        int configure(Vector<String> &, ErrorHandler *);

        int initialize(ErrorHandler* errh);

        /**
            @brief Initializes information in the bootstrapping process. 
            
            Initializes variables that are required for the SCION Switch to run.
            Not ALL the fields in the SCION Switch are initiated in this function
            but majority of the fields are initiated. 

            This function initializes two main types of fields. 
            
            1. Configuration File information
            
                The Config() is called and the configuration file for the Switch
                is parsed. Then the necessary accessor functions are called in
                order to fill up the fields.
                
            2. Topology File information
            
                The topology parser is created and the topology information is
                parsed into the fields. This is done by calling parseTology().
                Please refer to the spec of this function for more detail.
                
        */
        void initVariables(); 

        /**
            @brief Parses toplogy file from the file system

            This function loads and parses the Topology information. The
            TopoParser object is created and the topology file that is specified
            in the configuration file is loaded into the TopoParser by using
            loadTopoFile(). 

            When the topology file is loaded successfully, parseServers(),
            parseRouters(), parseClients() and  parseGateways() functions are called. 
            These function initializes the m_servers, m_routers, m_clients, and
            m_gateways respectively.  

            Also, the parseIFID2AID() is called to fill the ifid2addr mapping
            table, which is used when forwarding data packets and control packets
            out of the AD. 

            @note This function must be called only after the configuration file
            is parsed and the corresponding fields are filled. Specificatlly, this
            function requires m_sTopologyFile to be initialized.   
        */
        void parseTopology();
        
        /**
            @brief Sends packet to the specified port
            @param uint8_t* data The pointer that points to the buffer where the
            data is stored. This data will fill the click packet data section and
            will be forwarded. 
            @param uint16_t dataLength The length of the data to be forwarded. 
            @param int port The click port number to which the packet will be
            forwarded. 
            
            This is a wrapper function that wraps the sequence of creating a click
            packet and forwarding to the click port. The purpose of this function
            is the hide click code as much as possible so that later when we
            remove the click router from SCION, the overhead can be minimized.  
        */
        void sendPacket(uint8_t* data, uint16_t dataLength, int port);

        /**
            @brief when the necessary information required in switching packets
            are not present yet, the packet is stored in the pending table. 
        */
		void addToPendingTable(uint8_t* pkt, uint8_t & type, uint16_t & len, HostAddr & addr);

    private:
        Timer _timer;
        /**
            Configuration file.
        */
        String m_sConfigFile;
        /** Topology File */
        String m_sTopologyFile;

        //Temporary Value

        int m_iNumRouters;
        int m_iNumServers;
        int m_iNumClients;
        int m_iNumGateways;

        /** Log file name */
        char m_csLogFile[MAX_FILE_LEN];
        
        /** Mapping between the interface id and the connected AD's address */
        std::map<uint16_t, HostAddr> ifid2addr;
        //SLT: Map
        /** Mapping between the connected entity and the port number that it is
         * connected to */
		std::map<HostAddr, int> addr2port;   
        //std::map<uint64_t, int> addr2port;   
        
        /** 
            List of servers. Indexed by the server type 
        */
        std::multimap<int, ServerElem> m_servers;
        /**
            List of routers.
            Indext by the type of connected neighbors. 
        */
        std::multimap<int, RouterElem> m_routers;
        /** 
            List of gateways.
        */
        std::multimap<int, GatewayElem> m_gateways;         
        /**
            List of clients. 
        */
        std::map<int,ClientElem> m_clients;
       
        std::multimap<int, uint8_t*> pendingBS;
        std::multimap<int, uint8_t*> pendingPS;
        std::multimap<int, uint8_t*> pendingCS;
        
        /**
            Port number that client uses.
        */
        int m_iClientPort;
        /**
            Log level.
        */
        int m_iLogLevel;
        /**
            ADID that this switch belongs.
        */
        uint64_t m_uAdAid;
        /**
            SCION Print element for logging.
        */
        SCIONPrint* scionPrinter;
};

CLICK_ENDDECLS


#endif

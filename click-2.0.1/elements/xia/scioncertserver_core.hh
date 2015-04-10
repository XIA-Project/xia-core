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
//#include "scionipencap.hh"

CLICK_DECLS

/**
  @brief SCION Certificate Server Core element class

  This class is the implementation of the SCION TDC Certificate Server. SCION Certificate
  Server has two types, the TDC Certificate Server and Non-TDC Certificate Server. This
  class is the implementation for the TDC Certificate Server. The reference for
  the Non-TDC Certificate Server can be found in scioncertserver.hh. 

  The TDC Certificate server is responsible of two things. 

  1. Request/Reply of the certificates.

  There are two types of Certificate request/reply in SCION. One is from the
  local servers and the other is from the outside of the AD. The main routine
  for both cases are similar. When the requested target is in the certificate
  table, then the certificate server sends out the corresponding reply packet to
  the requesting servers or AD. 


  The major difference comes when sending the reply packets. If a certificate is
  found for a CERT_REQ_LOCAL packet then, the certificate server replys back
  with a CERT_REP_LOCAL packet which does not have a path inside. The only
  operation the certificate server does is source and destination address
  swapping. However, for the CERT_REQ packets, the certificate server reverses
  the path inside the packet. This must be done in order to send the reply
  packet back to the requester.  

  @note When the certificate is not found, the behavior is undefined for now. 

  2. Request/Reply of the ROT. 

  The ROT request/reply works in a similar way. When the ROT_LOCAL packet is
  received, the ROT_REP_LOCAL packet is sent with the addresses swapped. When
  the ROT_REQ packet is received, ROT_REP is sent out with the reversed path. 

 */
class SCIONCertServerCore : public Element { 
    public :
        SCIONCertServerCore():_timer(this), _task(this){};
        ~SCIONCertServerCore(){delete scionPrinter;};

        const char *class_name() const {return "SCIONCertServerCore";}
        const char *port_count() const {return "-/-";}
        const char *processing() const {return PUSH;}

        int configure(Vector<String> &, ErrorHandler *);
        int initialize(ErrorHandler* errh);
        void run_timer(Timer *timer);
        void push(int port, Packet *p);
        void sendHello();

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

          This function parses the ROT file using the ROTParser element.
          The path of the topology file is defined in the .conf file. The
          m_sROTFile value is set to this path in the initVariable()
          function. Only when a valid path is set, this function will extract
          the ROT information from the ROTParser into the m_cROT structure. 

          @note This function must be called after all the internal variables
          are initialized by initVariable() function.    
         */
        void parseROT();

        /**
          @brief Sends the packet to the given port number.
          @param uint8_t* data The packet data that will be sent. 
          @param uint16_t dataLength The length of the data.
          @param int port click port number

          This is a wrapper function that contains the click router routine that
          creates a new click packet with the given data and send the data to
          the given port.  
         */
        void sendPacket(uint8_t* data, uint16_t data_length, string dest);
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
          @brief Reverses the order of the path
          @param uint8_t* path The buffer that contains the original path. 
          @param uint8_t* output The buffer that will store the reversed path. 
          @param uint8_t hops The number of hops inside the original path. 

          This function reverses the order of the given path and stores it into
          the 'output' buffer. The usage of this function is for the Request and
          Reply packets. When the certificate server receives a Request packet
          from the external AD, then the request packet will contain a path that
          it followed. 

          In order to reply to the AD from which the packet is originated, the
          certificate server must reverse the path that was inside the request
          packet. Otherwise, the path request to the TDC will be necessary.
          However, for the initial case where non of the downstream ADs
          registered any paths, but requested for certificates to the upstream
          ADs, there will be no path available in the TDC to return to the
          certificate server.  
         */
        void reversePath(uint8_t* path, uint8_t* output, uint8_t hops);
        /**
          @brief Sends ROT to the specified address
          @param uint32_t ROT version number of the ROT contained inside the
          packet. 
          @param HostAddr srcAddr The source address of the requester. 


          This function sends out the ROT to the requester server. This function
          is only used to send certificate replys to the local servers. In other
          words, this function is not used to reply back to other ADs. For those
          external requests, processCertificateRequest() function is used.  
         */
        void sendROT();

        void processROTRequest(uint8_t * packet);
    private:

        Task _task;
        Timer _timer; 

        /** AID of certificate core server */        
        uint64_t m_uAid;
        /** Address of certificate core server */
        HostAddr m_Addr;
        /** ADID of the certificate core server */
        uint64_t m_uAdAid;                  //AD AID of pcb server
        /** TDID of the certificate core server */
        uint16_t m_uTdAid;

        // logger
        /** Log Level */
        int m_iLogLevel;
        /** SCION Printer */
        SCIONPrint* scionPrinter;

        // configuration file parameters
        String m_sConfigFile;                       /** configuration file name  */
        String m_sTopologyFile;                     /** topology file name       */
        char m_csCertFile[MAX_FILE_LEN];            /** certificate file name    */
        char m_csPrvKeyFile[MAX_FILE_LEN];          /** private key file name    */
        char m_csLogFile[MAX_FILE_LEN];             /** log file name            */
        char m_sROTFile[MAX_FILE_LEN];              /** ROT file name            */
        
        String m_AD;
        String m_HID;

        // RoT strcutrues
        /** ROT structure */
        ROT rot;
        /** ROT in raw bytes */
        char* curROTRaw;
        /** Length of the ROT */
        int curROTLen;

        /** List of Certificate Requests
            Mapps between the target and the certificate versions */
        std::map<uint64_t, int> certRequests;
        /**
            List of local servers.
            indexted by server type 
        */
        std::multimap<int, Servers> m_servers;
        /**
            List of local edge routers
            indexted by connected neighbor type 
        */
        std::multimap<int, RouterElem> m_routers;
        /**
            List of local gateways
        */
        std::multimap<int, GatewayElem> m_gateways;
        /**
            Mapping between the local ifid and the address of the neighbors.
        */
        std::map<uint16_t, HostAddr> ifid2addr;

        /** List of Port Info for IP tunneling */
        vector<portInfo> m_vPortInfo;	//address type and address of each interface
    };

CLICK_ENDDECLS


#endif

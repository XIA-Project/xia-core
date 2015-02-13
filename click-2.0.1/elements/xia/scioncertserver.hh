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
#include "scionipencap.hh"

CLICK_DECLS

/**
  @brief SCION Certificate Server element class

  This class is the implementation of the SCION Certificate Server. SCION Certificate
  Server has two types, the TDC Certificate Server and Non-TDC Certificate Server. This
  class is the implementation for the Non-TDC Certificate Server. The reference for
  the TDC Certificate Server can be found in scioncertserver_core.hh. 

  The Non-TDC Certificate server is responsible of two things. 

  1. Request/Reply of the certificates.

  There are two types of Certificate request/reply in SCION. One is from the
  local servers and the other is from the outside of the AD. The main routine
  for both cases are similar. When the requested target is in the certificate
  table, then the certificate server sends out the corresponding reply packet to
  the requesting servers or AD. When the certificate is not found, it creates
  the new request packet to the up stream. 

  The major difference comes when sending the reply packets. If a certificate is
  found for a CERT_REQ_LOCAL packet then, the certificate server replys back
  with a CERT_REP_LOCAL packet which does not have a path inside. The only
  operation the certificate server does is source and destination address
  swapping. However, for the CERT_REQ packets, the certificate server reverses
  the path inside the packet. This must be done in order to send the reply
  packet back to the requester.  

  2. Request/Reply of the ROT. 

  The ROT request/reply works in a similar way. When the ROT_LOCAL packet is
  received, the ROT_REP_LOCAL packet is sent with the addresses swapped. When
  the ROT_REQ packet is received, ROT_REP is sent out with the reversed path. 

 */
class SCIONCertServer : public Element { 
    public :
        SCIONCertServer():_timer(this), _task(this){};
        ~SCIONCertServer(){delete scionPrinter; delete curROTRaw;};

        /* click element related */
        const char *class_name() const {return "SCIONCertServer";}
        const char *port_count() const {return "-/-";}
        const char *processing() const {return PULL_TO_PUSH;}
        bool run_task(Task *task);
        int configure(Vector<String> &, ErrorHandler *);
        int initialize(ErrorHandler* errh);

        /* parsing works */
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
          @param String filename Filename of the ROT

          This function parses the ROT file using the ROTParser element.
          The path of the topology file is defined in the .conf file. The
          m_sROTFile value is set to this path in the initVariable()
          function. Only when a valid path is set, this function will extract
          the ROT information from the ROTParser into the m_cROT structure. 

          @note This function must be called after all the internal variables
          are initialized by initVariable() function.    
         */
        int parseROT(String filename = "");

        /**
          @brief Sends the packet to the given port number.
          @param uint8_t* data The packet data that will be sent. 
          @param uint16_t dataLength The length of the data.
          @param int port click port number
          @param fwd_type Forwarding type
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
          @brief Verifies the validity of the certificate
          @param uint8_t* pkt Packet that has the certificate
          @return Returns SCION_FAILURE on error, SCION_SUCCESS on success. 

          This function verifies the certificate certificate inside the packet
          using the certificates from the ROT file. In doing the verification,
          the polarssl library calls are made. Specifically this function uses
          x509parse_verify() function from the polarssl library. For more
          information about x509parse_verify() function, please refer to the
          polarssl documentation. 

          @note SCION uses the x509 structures and functions for all the
          certificate related operations. 
         */
        int verifyCert(uint8_t* pkt);

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
        int sendROT(uint32_t ROTVersion, HostAddr srcAddr);

        /**
          @brief Determines if the certificate of the given address is already
          requested. 
          @param uint64_t target The target AID of the owner of the requested
          certificate.  
          @param HostAddr The HostAddr of the requester. 
          @return Returns 1 if the request exists. 0 otherwise. 

          This function filters out duplicate requests of certificates. The
          beacon server maintains a table that contains pairs of requester
          addresses and certificate owner address. This function iterates
          through this table and check to see if the address inside the HostAddr
          has a mapping to the 'target' value. If such entry exists in the
          table, the function will return 1 and the certificate server will
          ignore the request. If not the function will return 0 and the
          certificate server will create a new entry for this request and will
          try to resolve this request. 

         */
        int isCertRequested(uint64_t, HostAddr);

        /**
          @brief Determines if the ROT of the given address is already
          requested. 
          @param uint64_t target The target AID of the owner of the requested
          ROT.  
          @param HostAddr The HostAddr of the requester. 
          @return Returns 1 if the request exists. 0 otherwise. 

          This function filters out duplicate requests of ROT. The
          beacon server maintains a table that contains pairs of requester
          addresses and ROT owner address. This function iterates
          through this table and check to see if the address inside the HostAddr
          has a mapping to the 'target' value. If such entry exists in the
          table, the function will return 1 and the ROT server will
          ignore the request. If not the function will return 0 and the
          certificate server will create a new entry for this request and will
          try to resolve this request. 

         */
        int isROTRequested(uint32_t, HostAddr);

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

        //SLN:
        /**
          @brief Handles ROT request packet from the local servers. 
          @param uint8_t* packet The buffer that contains the ROT request
          Pakcet. 

          This function handles all the incoming ROT_REQUEST_LOCAL packets from
          the local servers. The certificate server will put the curROTraw into
          the packet and send the packet to the requester server using the
          sendPacket() function.

          The reply packet will contain the current ROT with the version number.
          Also the packet type will be ROT_REPLY_LOCAL.   

         */
        void processROTRequest(uint8_t * packet);
        void processROTReply(uint8_t * packet);
        void processLocalROTRequest(uint8_t * packet);
        /**
          @brief Handles Certificate Request packet
          @param uint8_t* packet that contains the CERT_REQ packet. 

          This function process the certificate request packets from the
          downstream ADs. The packet type of the packet (param) will be the
          CERT_REQ and not CERT_REQ_LOCAL. 

          The Certificate Request packet contains the target ID, the ID of the
          owner of the targeted Certificate. This function will check to see if
          the current Certificate server has the requested certificate. If the
          certificate exists, the function will immeidately reply back to the
          requester AD by reversing the path inside the request packet. 

          Otherwise, the certificate server will hold the request packet and
          will send another request packet to its parents (up stream). 

          In cases where a single certificate request packet contains multiple
          targets, the certificate server will send a new request packet to its
          upstream for those targets that were not found in its certificate
          table.   
         */
        void processCertificateRequest(uint8_t * packet);
        /**
          @brief Handles the Certificate Reply packet
          @param uint8_t* packet The packet that has the certificate reply.

          This function handles the certificate reply packet that is received
          from the upstream. When a CERT_REPLY packet comes from the up stream
          AD, then this function is called. 

          This function first verifies all the certificates contained inside the
          packet by calling the verifyCert() function. If the verification
          passes, the function iterates through the certificate request table to
          see who has asked for this certificate. For all the requester, the
          certificate server creates a new certificate reply packet and sends
          the certificate reply packet.

          Also, when the received certificate is not in the certificate table of
          its own, the certificate server stores the certificate for future use.    
         */
        void processCertificateReply(uint8_t * packet);
        /**
          @brief Handles the Certificate request from the local servers. 
          @param uint8_t* packet The packet that contains the CERT_REQ_LOCAL
          packet. 

          This function handles the certificate requests from the local servers.
          This function operates in a similar way as the
          processCertificateRequest() function, but in the case when it sends
          the CERT_REP_LOCAL packets, this function does not call the reverse
          path function. Instead, it simply puts the source address of the
          request packet into the destination address of the reply packet and
          sends out to the switch.

          If the certificate is not found, this function creates a new CERT_REQ
          packet and sends the request packet to the upstream ADs just as
          processCertificateRequest() function does.    
         */
        void processLocalCertificateRequest(uint8_t * packet);
        void initializeOutputPort();

    private:
        Task _task;
        Timer _timer; 

        /** AID of certificate server */        
        uint64_t m_uAid;
        /** Address of certificate server */
        HostAddr m_Addr;
        /** ADID of the certificate server */
        uint64_t m_uAdAid;                  
        /** TDID of the certificate server */
        uint16_t m_uTdAid;
        /** Log Level */
        int m_iLogLevel;

        /* config files location */
        /** Configuration File Name */
        String m_sConfigFile;
        /** Topology File Name */
        String m_sTopologyFile;
        /** ROT File Name */
        String m_sROTFile;
        /** Certificate File name */
        String m_csCert;        //temp certificate file path
        /** Private Key File Name */
        String m_csPrvKey;      //temp private key file path
        String m_AD;
        String m_HID;

        /* ROT objects */
        /** ROT structure */
        ROT m_ROT;
        /** ROT in raw bytes */
        char* curROTRaw;
        /** Length of the ROT */
        int curROTLen;

        /** Log File Name */
        char m_csLogFile[MAX_FILE_LEN];
        /** SCION Printer */
        SCIONPrint* scionPrinter;

        /**
            List of hosts waiting for pending ROT requests 
            Mapping between the address of the host and the target. 
        */
        std::multimap<uint32_t, HostAddr> m_ROTRequests;
        /**
            List of hosts waiting for pending certificate Reqeusts.
            Mapping between the address of the host and the target.
        */
        std::multimap<uint64_t, HostAddr> m_certRequests; 
        /**
            List of local servers. 
            indexted by connected neighbor type 
        */	
        std::multimap<int, Servers> m_servers;
        //SL: unused elements but can be used later for key/cert. distribution
        /////////////////////////////////////////////////////////////////////
        /**
            List of local routers. 
            indexted by connected neighbor type 
        */	
        std::multimap<int, RouterElem> m_routers;
        /**
            List of local gateways. 
            indexted by server type 
        */	
        std::multimap<int, GatewayElem> m_gateways;         
        /////////////////////////////////////////////////////////////////////
        std::map<uint16_t, HostAddr> ifid2addr;
        /**
            SCION encap element for IP tunneling.
        */
        SCIONIPEncap * m_pIPEncap;
        /**
            Port information for IP tunneling that contains address type and
            address of each interface.
        */
        vector<portInfo> m_vPortInfo;
};

CLICK_ENDDECLS


#endif

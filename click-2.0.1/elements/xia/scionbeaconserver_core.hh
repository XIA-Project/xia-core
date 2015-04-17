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
//#include "scionipencap.hh"


CLICK_DECLS


/**
    @brief SCION Beacon Server Core element class.

    This class is the implementation of the TDC SCION Beacon Server. SCION Beacon
    Server has two types, the TDC Beacon Server and Non-TDC Beacon Server. This
    class is the implementation for the TDC Beacon Server. The reference for
    the Non-TDC Beacon Server can be found in scionbeaconserver.hh. 

    Compared to the Non-TDC Beacon Server, the TDC Beacon Server has simple task.
    It is responsible of creating new PCBs every PCB generation period and send
    them down the downstream ADs. The PCB generation time is defined in the .conf
    file and this time may vary depending on the policy of the AD.
    
    The creation of the PCBs are simple. The TDC Beacon Server creates a new
    packet for the new PCB and puts the links that are needed for the forwarding
    state. The links are defined as sets of information that is required at the
    end host to build paths. Detailed information about the links are decribed in
    scionbeacon.hh.   
*/

class SCIONBeaconServerCore : public Element {
    public :
        SCIONBeaconServerCore(): _timer(this), _task(this), _CryptoIsReady(false), 
            m_iPCBGenPeriod(1), m_bROTInitiated(false) {};
        
        ~SCIONBeaconServerCore()
        {
            if(_CryptoIsReady) rsa_free(&PriKey);
            delete scionPrinter;
            #ifdef _DEBUG_BS
            click_chatter("TDC BS (%s:%s): terminates correctly.\n", 
                m_AD.c_str(), m_HID.c_str());
	    #endif
        };
        
        /* click related functions */
        const char *class_name() const {return "SCIONBeaconServerCore";}
        const char *port_count() const {return "-/-";}
        const char *processing() const {return PUSH;}

        int configure(Vector<String> &, ErrorHandler *);
        int initialize(ErrorHandler* errh);
        void run_timer(Timer *timer);
        void push(int port, Packet *p);
        void sendHello();

        /**
            @brief Sends the packet to the given port number.
            @param uint8_t* data The packet data that will be sent. 
            @param uint16_t dataLength The length of the data.
            @param int port click port number

            This is a wrapper function that contains the click router routine that
            creates a new click packet with the given data and send the data to
            the given port.  
        */
        void sendPacket(uint8_t* data, uint16_t dataLength, string dest);
        
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
		bool getOfgKey(uint32_t timestamp, aes_context &actx);
        
        /**
            @brief Generates PCB packet

            Generates new PCB to propagate. This function generates new PCB
            packets that will be propagated. After the ROT, Topology file and the
            keys are initialized, the Beacon server can create new PCBs. It is the
            main wrapper of function calls from the scion beacon library such as
            generating Opaque fields and signing the packet.
            
            Similar to the link addition in the Non-TDC Beacon Servers, the link
            addition in the TDC Beacon servers depends on the policy engine of
            this AD. The policy engine chooses what links are going to be added to
            this new PCB and this function adds the link according to the policy.
        */
        bool generateNewPCB();    // PCB packet generation
   
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
        
    private:
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
        bool parseROT();
        
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

        /** True if ROT is initiated, False if not. */     
        bool m_bROTInitiated;
        /** Opaque Field generation key table in aes_context */
        std::map<uint32_t, aes_context*> m_OfgAesCtx; 
        
        Timer _timer;
        Task _task;

        String m_AD;
        String m_HID;

        /** RoT structure */
        ROT m_cROT;

        String m_sConfigFile;                       /** configuration file name  */
        String m_sTopologyFile;                     /** topology file name       */
        char m_csCertFile[MAX_FILE_LEN];            /** certificate file name    */
        char m_csPrvKeyFile[MAX_FILE_LEN];          /** private key file name    */
        char m_csLogFile[MAX_FILE_LEN];             /** log file name            */
        char m_sROTFile[MAX_FILE_LEN];              /** ROT file name            */

        uint64_t m_uAid;                            /** AID (address)*/
        HostAddr m_Addr;                            /** Address in HostAddr type */
        uint64_t m_uAdAid;                          /** Administrative Ddomain AID (AD #)*/
        uint16_t m_uTdAid;                          /** Trust Domain AID*/
        uint8_t m_uMkey[MASTER_SECRET_KEY_SIZE];    /** Master secret key*/

        rsa_context PriKey;                         /** cryptography object, now we use RSA*/
        
        // flags
        bool m_bRsaCtxLoad;  /** True if private key is loaded to RSA_CTX */
        bool _CryptoIsReady; /** True if Cryptography is ready */

        int m_iLogLevel;/**< Log Level */
        SCIONPrint* scionPrinter;/**< SCION Printer */            
        
        /**
            Period for Sending PCB packets
        */
        int m_iPCBGenPeriod;
        
        // OFG keys
        ofgKey m_currOfgKey;                    /**<Current Opaque Field Generation Key */
        ofgKey m_prevOfgKey;                    /**<Previous Opaque Field Genration Key */

        /** 
            List of servers 
            Indexed by the server type.
        */
        std::multimap<int, Servers> m_servers;
        /**
            List of routers.
            Indexed by the connected neighbor type.
        */
        std::multimap<int, EgressIngressPair> m_routepairs;

};

CLICK_ENDDECLS


#endif



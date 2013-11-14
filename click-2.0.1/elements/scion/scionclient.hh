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

#ifndef SCIONCLIENT_HEADER_HH_
#define SCIONCLIENT_HEADER_HH_

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
#include "scionprint.hh"
#include "packetheader.hh"
#include "scionbeacon.hh"
#include "topo_parser.hh"
#include "config.hh"
CLICK_DECLS

struct end2end{
    uint32_t upTs;
    uint32_t downTs;
    uint16_t totalLength;
    uint16_t hops;
    uint8_t* msg;
    int sent;
};

struct peer{
    uint64_t aid;
    uint64_t naid;
    uint16_t srcIfid;
    uint16_t dstIfid;  
};

struct p{
    uint16_t hops;
    uint16_t num_peers;
    uint32_t timestamp;
    std::vector<uint64_t> path;  //vector of ADAID
    std::multimap<uint64_t, peer> peers;
    uint16_t pathLength;
    uint8_t* msg; 

};

class SCIONClient : public Element { 
    public :
        SCIONClient(): _timer(this), _task(this){
            m_bRval= false;
            m_bMval = false;};
        ~SCIONClient(){ //delete scionPrinter;
        };
       
//        const char *flow_code()  const {return "x/x";}
        const char *class_name() const {return "SCIONClient";}
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


        bool run_task(Task *);

        void run_timer(Timer *); 
       
        bool peer_cmp(peer up, peer down);
        
//        void push(int, Packet*); 
        const char *processing() const {return PULL_TO_PUSH;}

        int configure(Vector<String> &, ErrorHandler *);

        int initialize(ErrorHandler* errh);
        
        void parse(uint8_t* pkt, int type);

        void printUpPaths();
        void printDownPaths();

        void getAllPaths();
        
        void getCorePath(p downPath, p upPath);

        void getXovrPath(p downPath, p upPath);

        void getPeerPath(p downPath, p upPath);

        void sendToAllPath();

        bool sendToPath();
        
        void printAllPath();
    
        void printPath(uint8_t*, uint16_t);
        
        void clearDownPaths();

        void clearUpPaths();

        void clearPaths();
        
		//SL: added to locate PS
        void parseTopology();

    private:
        uint64_t m_uAid;
        uint64_t m_uTarget;
        uint64_t m_uPsAid;
        uint64_t m_uAdAid;
        int m_iLogLevel;
        Timer _timer;
        Task _task;
        multimap<uint64_t, p> downpath;
        multimap<uint64_t, p> uppath;        
        multimap<uint64_t, end2end> paths; 
        bool m_bRval;
        bool m_bMval;        
        String m_sLogFile;
        
		//SL: added to locate Path Server
		//Client should be limited to access PS
        std::multimap<int, ServerElem> m_servers;
        std::multimap<int, GatewayElem> m_gateways;         
        String m_sTopologyFile;

        int printCounter;

        SCIONPrint* scionPrinter;
};

CLICK_ENDDECLS


#endif

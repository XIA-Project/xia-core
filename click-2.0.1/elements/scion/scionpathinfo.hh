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

#ifndef SCION_PATHINFO_HH
#define SCION_PATHINFO_HH

#include <click/element.hh>
#include <click/hashcontainer.hh>
#include <click/hashallocator.hh>
#include <click/sync.hh>
#include <click/timer.hh>
#include <click/list.hh>
#include <map>
#include <vector>
#include <list>
#include "packetheader.hh"

CLICK_DECLS

//This should be moved to define.hh or set by a conf. parameter
#define MAX_HOST_OF_STORE	3
typedef uint64_t ADAID;

class SCIONPathInfo : public Element { 
public:
	//SL: added to find the shortest e2e path
	///////////////////////////////////////////////////////////
    struct UpPathAD {//Charles
        uint8_t type; //normal, blocked, tdc, dst, src
        uint16_t hops;
        std::vector<halfPath*> path_ptr;
    };
    
    struct DownPathAD {//Charles
        uint8_t type;
        uint16_t hops;
        std::vector<halfPath*> path_ptr;
        std::vector<uint64_t> peer_ad;
    };

    typedef std::map<ADAID, UpPathAD> UpPathADTable;
    typedef std::map<ADAID, DownPathAD> DownPathADTable;
	///////////////////////////////////////////////////////////


    SCIONPathInfo();
    ~SCIONPathInfo();

    const char *class_name() const { return "SCIONPathInfo"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const		{ return true; }
	//SL: why is this necessary?
	//probably, if paths are read from internal DB?
    void take_state(Element *, ErrorHandler *);
    void cleanup(CleanupStage);

    void clearDownPaths();
    void clearUpPaths();

    void parse(const void *, int);

	//SL: added to find the shortest path
	//////////////////////////////////////////////////////////
    void addUpPath(halfPath&);
    void addDownPath(halfPath&, DownPathADTable&);
    void deleteUpPath(halfPath&);
    void deleteDownPath(halfPath&, DownPathADTable&);
    bool initAdTable(ADAID, ADAID);

    bool getShortestPath(DownPathADTable&, fullPath&);
    void constructFullPath(ADAID, ADAID, halfPath&, halfPath&, uint16_t, uint16_t, uint8_t, fullPath&); 

    bool printUpDownPaths(ADAID src, ADAID dst);
    void printFullPath(fullPath&);
    void printAdTable(DownPathADTable&);
	//////////////////////////////////////////////////////////

    void getCorePath(halfPath&, halfPath&, fullPath&);
    bool get_path(ADAID, ADAID, fullPath&);

	bool storeOpaqueField(HostAddr &addr, fullPath &of);
	bool storeOpaqueField(uint64_t &adaid, fullPath &of);
	bool storeOpaqueField(in_addr_t &ip, fullPath &of);
	bool reverseOpaqueField(fullPath &of);

	//SL: Addr to opaque field mapping for incoming packets //(for revers path construction)
	std::map<HostAddr, list<fullPath> > m_inOF; 
	//SL: AD to opaque field mapping for outgoing packets
	std::map<ADAID, list<fullPath> > m_outOF; 

	//SL: This is for multi-threaded elements
	//for now, our implementation runs in a single-thread mode
	//(since usermode click only supports single threaded elements)
	//Yet, this needs to be used in the kernel mode to sync elements
    ReadWriteLock _lock;

  private:
    std::map<ADAID, std::vector<halfPath*> > m_vDownpaths;
    std::map<ADAID, std::vector<halfPath*> > m_vUppaths;

	//SL: added to find the shortest path
	//////////////////////////////////////////////////////////////
    UpPathADTable m_uppathAdTable;
    std::map<ADAID, DownPathADTable> m_downpathAdTable;
	//////////////////////////////////////////////////////////////

	//SL: We may need flowid to OF mapping to reverse paths
	//However, flowid has not been defined yet.
	//Reserved this for future use
	std::map<uint64_t, fullPath> m_fInOF;
	std::map<uint64_t, fullPath> m_fOutOF;
};


CLICK_ENDDECLS
#endif

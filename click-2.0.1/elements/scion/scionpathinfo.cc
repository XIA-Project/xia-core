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

#include <click/config.h>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/args.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include "scionpathinfo.hh"
#include "packetheader.hh"
#include <algorithm>

CLICK_DECLS

SCIONPathInfo::SCIONPathInfo()
{
}

SCIONPathInfo::~SCIONPathInfo()
{
}

int
SCIONPathInfo::configure(Vector<String> &conf, ErrorHandler *errh)
{
	//SL: what is this for???
	//seems to limit the lifetime of SCIONPathInfo
	//this would be moved to e2eOF's cache_time
    Timestamp timeout(300);
    if (Args(conf, this, errh)
	.read("TIMEOUT", timeout)
	.complete() < 0)
	   return -1;
    
	return 0;
}

/*SL: cleanup cleans up an element like a destructor, 
yet can be called long before a destructor is called. 
clear all up- and down-paths
*/
void
SCIONPathInfo::cleanup(CleanupStage)
{
    clearDownPaths();
    clearUpPaths();
}

/*SL: clear all downpaths
*/
void
SCIONPathInfo::clearDownPaths()
{   
    for (std::map<ADAID, std::vector<halfPath*> >::iterator it = m_vDownpaths.begin(); it != m_vDownpaths.end(); ++it) {
        for (std::vector<halfPath*>::iterator jt = it->second.begin(); jt != it->second.end(); ++jt) {
            if ((*jt)->path_marking)
                delete [] (*jt)->path_marking;
			delete *jt;
        }
    }
    m_vDownpaths.clear();
	m_downpathAdTable.clear();
}

/*SL: clear all uppath
*/
void
SCIONPathInfo::clearUpPaths()
{   
	for (std::map<ADAID, std::vector<halfPath*> >::iterator it = m_vUppaths.begin(); it != m_vUppaths.end(); ++it) {
        for (std::vector<halfPath*>::iterator jt = it->second.begin(); jt != it->second.end(); ++jt) {
            if ((*jt)->path_marking)
                delete [] (*jt)->path_marking;
			delete *jt;
        }
    }
    m_vUppaths.clear();
    m_uppathAdTable.clear(); //clear up-path AD table
}

/*SL: 
Under the assumption that there's only one shared SCIONPathInfo element in a system,
take_state read SCIONPathInfo instance from the memory and initialize up- an down-path. 
*/
void
SCIONPathInfo::take_state(Element *e, ErrorHandler *errh)
{
    SCIONPathInfo *pinfo = (SCIONPathInfo *)e->cast("SCIONPathInfo");
    if (!pinfo)
	   return;
    if (m_vUppaths.size() > 0 || m_vDownpaths.size() > 0 /* || _paths.size() > 0 */) {
	   errh->error("late take_state");
	   return;
    }

    m_vUppaths.swap(pinfo->m_vUppaths);
    m_vDownpaths.swap(pinfo->m_vDownpaths);
    //_paths.swap(pinfo->_paths);
}

void
SCIONPathInfo::parse(const void *p, int type)
{
	//struct for path storage
    halfPath *received_path_ptr = new halfPath();
    memset(received_path_ptr, 0, sizeof(halfPath)); 
    
	uint8_t srcLen = SPH::getSrcLen((uint8_t *)p);
	uint8_t dstLen = SPH::getDstLen((uint8_t *)p);
    //extracting information from packet header

	//SL: Caution: downpath includes OF to TDC
	uint8_t hdrLen = SPH::getHdrLen((uint8_t *)p);
    uint8_t *pkt = (uint8_t *)p + hdrLen;
    uint16_t hops = 0;

    pathInfo* pi = (pathInfo*)(pkt);

	/*SL: get the first OF, which is the timestamp */
   	pkt += PATH_INFO_SIZE;
    specialOpaqueField * pFOF = (specialOpaqueField *) pkt;
    hops = pFOF->hops; 

	#ifdef _SL_DEBUG_GW
	printf("Parsing received path: Type:%d (0:uppath, 1:downpath), hops: %d\n", type, hops);
	printf("Path info: info=%x, ts=%d\n",pFOF->info,pFOF->timestamp);
	#endif

    //1. extract half path information and store it to halfPath struct
	//iterate through payload and get hop information
    pkt += OPAQUE_FIELD_SIZE;

    pcbMarking *mrkPtr = (pcbMarking*) pkt;
	//PCB contains two special OF: timestamp and rot version
    
	//SL: for each AD marking...
    for (int i = 0; i < hops; ++i) {
		//1.1 add ADAID to the path
        received_path_ptr->path.push_back(mrkPtr->aid);
		#ifdef _SL_DEBUG_GW
		printf("=> %llu", mrkPtr->aid);
		#endif
		++ received_path_ptr->hops;
        
		//1.2 handle if there's any peering link
        uint8_t num_peers = (mrkPtr->blkSize-PCB_MARKING_SIZE) / PEER_MARKING_SIZE;
		if(num_peers) {
			uint8_t * pkt2 = pkt + PCB_MARKING_SIZE;
			peerMarking * peerPtr = (peerMarking *)pkt2;

			//1.2.1 extracting peer information
			for (int j = 0; j < num_peers; ++j) {
				Peer new_peer;
				memset(&new_peer, 0, sizeof(Peer));
				//SLP: the following code should be replace with a single 64bit INT assignment
				//left unmodified for debugging purpose
				new_peer.aid = mrkPtr->aid;
				new_peer.naid = peerPtr->aid;
				new_peer.srcIfid = peerPtr->ingressIf;
				new_peer.dstIfid = peerPtr->egressIf; 
				received_path_ptr->peer.insert(std::pair<uint64_t, Peer>(mrkPtr->aid, new_peer));
				
				received_path_ptr->num_peers++;
				pkt2 += PEER_MARKING_SIZE;
				peerPtr = (peerMarking*)pkt2;
			}
		}

		//1.3. move pointer to the next AD's marking
		#ifdef _SL_DEBUG_GW
		printf(" (%dB) =>", mrkPtr->blkSize);
		#endif
        pkt += mrkPtr->blkSize;//+mrkPtr->sigLen;
        mrkPtr = (pcbMarking*)pkt;
    }

	#ifdef _SL_DEBUG_GW
	printf("Store %d path: 0: uppath, 1:downpath\n", type);
	#endif
    //2. Store extracted path information
	//2.1 storing down path
    if (type) {
        uint16_t info_length = pi->totalLength;
		//SL: why all information is copied to opaque field???
		//probably, opaque_field * is not the opaque field
        received_path_ptr->path_marking = new uint8_t[ info_length ];

		pkt = (uint8_t*)p + hdrLen + PATH_INFO_SIZE;
        memcpy(received_path_ptr->path_marking, (uint8_t *)pkt, info_length);
        received_path_ptr->length = info_length;
		//get special opaque field...
		specialOpaqueField * pSO = (specialOpaqueField *)pkt;
        received_path_ptr->timestamp = pSO->timestamp;
        
		assert(received_path_ptr->path.size() > 0);
        
		ADAID endpoint = received_path_ptr->path[received_path_ptr->path.size() - 1];
        ADAID core = received_path_ptr->path[0];

		#ifdef _SL_DEBUG_GW
		printf("Storing down path: len = %d,endpoint = %llu, core = %llu\n", info_length,endpoint,core);
		#endif

		//SL: store to the m_vDownpaths with the endpoint ADAID as a key...
		//2.1.1 create a new entry
		if (m_vDownpaths.find(endpoint) == m_vDownpaths.end()) {
            std::vector<halfPath*> v;
            v.push_back(received_path_ptr);
            m_vDownpaths[endpoint] = v;
		//2.1.2 add to the existing entry (SL: need to check if the table overflows)
        } else {
            m_vDownpaths[endpoint].push_back(received_path_ptr);
        }
        
		#ifdef _SL_DEBUG_GW
        printf("\t PathInfo: save down-path from %llu to %llu.\n", core, endpoint);
		#endif
		addDownPath(*(m_vDownpaths[endpoint].back()), m_downpathAdTable[endpoint]);
    
    //2.2 storing up path
	//SL: almost redundant with the previous block 
	//merge these together after changing the message format...
    } else {
		uint16_t info_length = SPH::getTotalLen((uint8_t *)p) - hdrLen - PATH_INFO_SIZE;
		#ifdef _SL_DEBUG_GW
		printf("Storing up path: len = %d\n", info_length);
		#endif
        received_path_ptr->path_marking = new uint8_t[ info_length ];
		
		pkt = (uint8_t*)p + hdrLen + PATH_INFO_SIZE;
        memcpy(received_path_ptr->path_marking, (uint8_t *)pkt, info_length);
        received_path_ptr->length = info_length;
		//get special opaque field...
		specialOpaqueField * pSO = (specialOpaqueField *)pkt;
        received_path_ptr->timestamp = pSO->timestamp;
        
		assert(received_path_ptr->path.size() > 0);
        
        ADAID endpoint = received_path_ptr->path[received_path_ptr->path.size() - 1];
        ADAID core = received_path_ptr->path[0];
        
		if (m_vUppaths.find(endpoint) == m_vUppaths.end()) {
            std::vector<halfPath*> v;
            v.push_back(received_path_ptr);
            m_vUppaths[endpoint] = v;
        } else {
            m_vUppaths[endpoint].push_back(received_path_ptr);
        }

		#ifdef _SL_DEBUG_GW
        printf("\t PathInfo: save up-path from %llu to %llu.\n", core, endpoint);
		#endif
        addUpPath(*(m_vUppaths[endpoint].back()));
    }
}

/*SL: SCIONPathInfo::get_path returns an end-2-end path 
for a given src and dst AD pair
*/
bool 
SCIONPathInfo::get_path(ADAID src, ADAID dst, fullPath& endpath) 
{
    std::map<ADAID, std::vector<halfPath*> >::iterator it_down = m_vDownpaths.find(dst);
    std::map<ADAID, std::vector<halfPath*> >::iterator it_up = m_vUppaths.find(src);

	//check if both up- and down-path exist
    if (it_up != m_vUppaths.end() && it_down != m_vDownpaths.end()) {
		//once all path construction is done, an e2e path should be chosen by the client (or endpoint app.)
        if (!getShortestPath(m_downpathAdTable[dst], endpath)) {
            // If getShortestPath didn't find a shortcut path, then we use the path through the TDCore
			//make sure that opaque fields are not polluted from the previous function call;
			//i.e., in finding the shortest, non-TDC path
			if(endpath.opaque_field)
				delete [] endpath.opaque_field;

        	getCorePath(*((*it_down).second[0]), *((*it_up).second[0]), endpath);
		}
        return true;
    } else {
        return false;
    }
}

/*
 Initialize UpPathADTable and DownPathADTables from all the up-paths and down-paths available
   - only OPAQUE FIELDS, HOPS and TIMESTAMPS in the Path are used. 
   - it makes the function rely very little on the Path data structure.
*/
bool
SCIONPathInfo::initAdTable(ADAID src, ADAID dst) {

    std::map<ADAID, std::vector<halfPath*> >::iterator it_down = m_vDownpaths.find(dst);
    std::map<ADAID, std::vector<halfPath*> >::iterator it_up = m_vUppaths.find(src);
    // clear before initialization
    m_uppathAdTable.clear();
    m_downpathAdTable.clear();     

    // UP-PATHS
	std::vector<halfPath*>::iterator it_up_path;
	//for each up-path
    for (it_up_path = it_up->second.begin(); it_up_path != it_up->second.end(); ++it_up_path) { 
        addUpPath(*(*it_up_path));     
    }

    // DOWN-PATHS
	std::vector<halfPath*>::iterator it_down_path;
	//for each down-path
    for (it_down_path = it_down->second.begin(); it_down_path != it_down->second.end(); ++it_down_path) { 
        addDownPath(*(*it_down_path), m_downpathAdTable[dst]);
    }
   return true;
}

/*
 Delete an up-path from the up-Path AD table
*/
void
SCIONPathInfo::deleteUpPath(halfPath& path) {
//SLP:
//this should be re-written
//don't know how this worked before...
    uint8_t *ptr = path.path_marking;
    pcbMarking *mrkPtr = (pcbMarking*) ptr;
    uint16_t hops = path.hops;
    for (int i = hops; i>0; --i) { //for each ad on the path
        UpPathAD& up_path_ad = m_uppathAdTable[mrkPtr->aid];
        
		if (up_path_ad.hops == i) {
            std::vector<halfPath*>::iterator it = std::find(up_path_ad.path_ptr.begin(), 
				up_path_ad.path_ptr.end(), &path);
            
			if (it != up_path_ad.path_ptr.end()) {
                if (up_path_ad.path_ptr.erase(it) == up_path_ad.path_ptr.end()) {}; //TBD-exception
            }
            
			if (up_path_ad.path_ptr.empty()) { //erase ad if it's empty
                m_uppathAdTable.erase(mrkPtr->aid);
            }
        }
        ptr += mrkPtr->blkSize;
        mrkPtr = (pcbMarking*) ptr;
    }
}

/*
 Delete a down-path from the down-Path AD table corresponding to the path's destination
*/
void
SCIONPathInfo::deleteDownPath(halfPath& path, DownPathADTable& down_path_ad_table) {
//SLP:
//this should be re-written
//don't know how this worked before...
    uint8_t* ptr = path.path_marking;
    pcbMarking * mrkPtr = (pcbMarking*) ptr;
    uint16_t hops = path.hops;
    for (int i = hops; i>0; --i) { //for each ad on the path
        DownPathAD& down_path_ad = down_path_ad_table[mrkPtr->aid];
        
		if (down_path_ad.hops == i) {
            down_path_ad.path_ptr.erase(std::find(down_path_ad.path_ptr.begin(), 
				down_path_ad.path_ptr.end(), &path)); //TBD-exception
            
			if (down_path_ad.path_ptr.empty()) { //erase ad if it's empty
                down_path_ad_table.erase(mrkPtr->aid);
            }
        }
        ptr += mrkPtr->blkSize;
        mrkPtr = (pcbMarking*) ptr;
    } 
}

/*
 Add ADs on a down path to that path's destination's DownPathADTable
*/
void
SCIONPathInfo::addDownPath(halfPath& path, DownPathADTable& down_path_ad_table) {
    uint8_t* dwPtr = path.path_marking +OPAQUE_FIELD_SIZE; //first OF is the TS of this PCB
    pcbMarking *dwMrkPtr = (pcbMarking*) dwPtr;
	#ifdef _SL_DEBUG_GW
	printf("path.hops = %d, aid = %llu\n", path.hops, dwMrkPtr->aid);
	 #endif
    for (uint16_t downHop = path.hops; downHop>0; --downHop) { //for each aid in a down-path
       uint64_t aid = dwMrkPtr->aid;
	   #ifdef _SL_DEBUG_GW
	   printf("\tAD%d (hop:%d) =>",aid,downHop);
	   #endif
       if (down_path_ad_table.find(aid) == down_path_ad_table.end()) { //if aid is not in the table
           //add path pointer
           DownPathAD down_path_ad;
           down_path_ad.hops = downHop;
		   if(downHop==path.hops)
		   		down_path_ad.type = TDC_AD;
			else
				down_path_ad.type = NON_TDC_AD;
           down_path_ad.path_ptr.push_back(&path);
           //add peer ADs connected with this AD
           uint8_t* dwPtr2 = dwPtr + PCB_MARKING_SIZE;
           peerMarking * peerMrkPtr = (peerMarking*) dwPtr2;
           uint16_t num_peers = (dwMrkPtr->blkSize-PCB_MARKING_SIZE) / PEER_MARKING_SIZE;
           for (int j = 0; j < num_peers; ++j) {
               if (std::find(down_path_ad.peer_ad.begin(), down_path_ad.peer_ad.end(),peerMrkPtr->aid)
			   		==down_path_ad.peer_ad.end()) { //TBD-speed
                   down_path_ad.peer_ad.push_back(peerMrkPtr->aid);
               }
               dwPtr2 += PEER_MARKING_SIZE;
               peerMrkPtr = (peerMarking*) dwPtr2;
           }
           //add ad to map
           down_path_ad_table[aid] = down_path_ad;           
       }
       else { //if aid is in the table
           DownPathAD& down_path_ad = down_path_ad_table[aid];
           if (down_path_ad.hops > downHop) { //lower hops
               down_path_ad.hops = downHop;
               down_path_ad.path_ptr.clear();
               down_path_ad.path_ptr.push_back(&path);
           }
           else if (down_path_ad.hops == downHop) { //equal hops
               down_path_ad.path_ptr.push_back(&path);
           }
       }
       dwPtr += dwMrkPtr->blkSize;
       dwMrkPtr = (pcbMarking*) dwPtr;
    }
}

/*
 Add ADs on an up-path to the UpPathADTable
*/
void 
SCIONPathInfo::addUpPath(halfPath& path) {
    uint8_t* upPtr = path.path_marking + OPAQUE_FIELD_SIZE; //the first OF is the TS of this PCB
    pcbMarking * upMrkPtr = (pcbMarking*) upPtr;
    for (uint16_t upHop = path.hops; upHop>0; --upHop) { //for each aid in an up-path
       uint64_t aid = upMrkPtr->aid;
       if (m_uppathAdTable.find(aid) == m_uppathAdTable.end()) { //if aid is not in the table
           //add path pointer
           UpPathAD up_path_ad;
           up_path_ad.hops = upHop;
           up_path_ad.path_ptr.push_back(&path);
           //add ad to map
           m_uppathAdTable[aid] = up_path_ad;
       }
       else { //if aid is in the table.
           UpPathAD& up_path_ad = m_uppathAdTable[aid];
           if (up_path_ad.hops > upHop) { //lower hops
               up_path_ad.hops = upHop;
               up_path_ad.path_ptr.clear();
               up_path_ad.path_ptr.push_back(&path);
           }
           else if (up_path_ad.hops == upHop) { //equal hops
               up_path_ad.path_ptr.push_back(&path);
           }
       }
       upPtr += upMrkPtr->blkSize;
       upMrkPtr = (pcbMarking*) upPtr;
   }
}

/*
SL: Get shortest path. If there is no shortcut return false.
   - first, find an AD-level shortest path 
   - second, construct a series of opaque fields that correspond to the AD path
   	 which is implemented in constructFullPath()
*/
bool
SCIONPathInfo::getShortestPath(DownPathADTable& down_path_ad_table, fullPath& endpath) {
	//SL: MAX_HOPS needs to be defined in define.hh
    uint64_t upAD = 0, dwAD = 0;
    uint8_t end2end_path_type = PATH_TYPE_TDC; //0- TDC, 1-CROSSOVER, 2-PEER
    uint16_t end2end_path_hops = MAX_AD_HOPS;
    uint16_t end2end_up_hops = MAX_AD_HOPS;
    uint16_t end2end_down_hops = MAX_AD_HOPS; //TBD-typedef INF number?
    
    //traverse down-path ADs and search for cross-over ADs or peering links to ADs on up-path
	//SL: the first entry in the down_path_ad_table is the destination AD
	//starting from the destination AD, traverse toward the TDC to find a shortcut
    for (std::map<ADAID, DownPathAD>::iterator it_down_ad = down_path_ad_table.begin(); 
		it_down_ad != down_path_ad_table.end(); ++it_down_ad) {
        //1. search cross-over AD (i.e., common AD of up & down paths) on up-paths
        std::map<ADAID, UpPathAD>::iterator it_up_ad_xover = m_uppathAdTable.find(it_down_ad->first);
        if (it_up_ad_xover != m_uppathAdTable.end()) { //cross-over shortcut
           uint8_t path_hops_xover = it_down_ad->second.hops + it_up_ad_xover->second.hops - 1;
		   //1.1 update the shortest path if found
           if (path_hops_xover < end2end_path_hops) {
               upAD = it_up_ad_xover->first;
               dwAD = it_down_ad->first;

			   #ifdef _SL_DEBUG_GW
			   printf("AD%d:type(%d)\n",it_down_ad->first,it_down_ad->second.type);
			   #endif

				//SL: Check if this ad is a TDC_AD
				if(it_down_ad->second.type == TDC_AD){
               		end2end_path_type = PATH_TYPE_TDC;
               		continue;
				}
				else
               		end2end_path_type = PATH_TYPE_XOVR;
			   //SL: next two lines might be unnecessary
               end2end_up_hops = it_up_ad_xover->second.hops;
               end2end_down_hops = it_down_ad->second.hops;
               end2end_path_hops = end2end_up_hops + end2end_down_hops;
           }
        }
        
		//2. search peering links to ADs on up-paths
        for (std::vector<uint64_t>::iterator it_peer = it_down_ad->second.peer_ad.begin(); 
			it_peer != it_down_ad->second.peer_ad.end(); it_peer++) {
            std::map<ADAID, UpPathAD>::iterator it_up_ad_peer = m_uppathAdTable.find(*it_peer);
            if (it_up_ad_peer != m_uppathAdTable.end()) { //peer shortcut
                uint8_t path_hops_peer = it_down_ad->second.hops + it_up_ad_peer->second.hops;
                if (path_hops_peer < end2end_path_hops) {
                   upAD = it_up_ad_peer->first;
                   dwAD = it_down_ad->first;
                   end2end_path_type = PATH_TYPE_PEER;
			   	   //SL: next two lines might be unnecessary
                   end2end_up_hops = it_up_ad_peer->second.hops;
                   end2end_down_hops = it_down_ad->second.hops;
                   end2end_path_hops = end2end_up_hops + end2end_down_hops;
                }
            }
        }
    }

	//3. If a shortcut (i.e., crossover or peer) is found, construct an e2e path with the corresponding opaque fields.
    if (end2end_path_type != PATH_TYPE_TDC) { 
        
		//3.1 get up-path and down-path references for full-path construction 
        std::map<ADAID, UpPathAD>::iterator itr_up_ad = m_uppathAdTable.find(upAD);
        std::map<ADAID, DownPathAD>::iterator itr_down_ad = down_path_ad_table.find(dwAD);
		
		//3.2 pick the first one of the available paths for the turning point
        halfPath& upPath = *(*(itr_up_ad->second.path_ptr.begin())); 
        halfPath& downPath = *(*(itr_down_ad->second.path_ptr.begin()));
        
		//3.3 construct opaque-field path based on AD-level info
        constructFullPath(upAD,dwAD,upPath,downPath,end2end_up_hops,end2end_down_hops,end2end_path_type,endpath);
        return true;
    } else {
		#ifdef _SL_DEBUG_GW
        printf("A shortcut path doesn't exist\n");
		#endif
        return false;
    }
}

/*
SL: Construct a non-TDC (i.e., shortcut) full-path using path type and up-path, down-path references
   - this function does not use the path table and AD table structures
*/
void
SCIONPathInfo::constructFullPath(ADAID upAD, ADAID dwAD, halfPath& upPath, halfPath& downPath, 
uint16_t end2end_up_hops, uint16_t end2end_down_hops, uint8_t end2end_path_type, fullPath& endpath) {

    uint8_t* upPtr = upPath.path_marking;
    uint8_t* dwPtr = downPath.path_marking;
	uint8_t dwHop = end2end_down_hops;
   	uint8_t upHop = end2end_up_hops;
    
	//total up/down path length in terms of # of OFs
    uint16_t up_length; //add a special OF, i.e., up timestamp
    uint16_t down_length; //add a special OF, i.e., down timestamp

    pcbMarking* mrkPtr = (pcbMarking*)upPtr;  
   	
	//SLC: this part needs to be integrated with the TDC path routine
	//since many lines are same as those of TDC path
	//However, postpone it until functionality is verified...
	//1. a path through a crossover AD
	if (end2end_path_type == PATH_TYPE_XOVR) { //cross-over
		#ifdef _SL_DEBUG_GW
		printf("\nConstruct a XOVR path\n");
		#endif
    	up_length = (upHop+2) * OPAQUE_FIELD_SIZE; //add OFs for a special OF and upstream AD
    	down_length = (dwHop+2) * OPAQUE_FIELD_SIZE; //add OFs for a special OF and upstream AD
		uint8_t e2ePath[(up_length+down_length)*OPAQUE_FIELD_SIZE];
		
		//Up-path
		//1.1 first marking is the special OF
    	specialOpaqueField * pTS = (specialOpaqueField*) upPtr;
		specialOpaqueField * pSO = (specialOpaqueField*) e2ePath;
		*pSO = *pTS;
		//1.1.1 info must be set by client...
		pSO->info = 0xc0;// indicates normal shortcut through a common AD
	
		//1.2. then mark a series of the OF of the selected path
		//Up-path
		#ifdef _SL_DEBUG_GW
		printf("Up path in reverse order (from TDC) upPath.hops = %d, downPath.hops = %d, upHop=%d, dwHop=%d\n",
			upPath.hops, downPath.hops,upHop,dwHop);
		#endif
        upPtr += OPAQUE_FIELD_SIZE;
        mrkPtr = (pcbMarking*)upPtr;
		//building opaque field list for the up path
        uint16_t offset = (upHop+1)*OPAQUE_FIELD_SIZE; //since the OF of crossover AD should be included
		opaqueField * pOF;

        for(int i=0;i<upPath.hops;i++){
            //ignore the unused hops
            if(i<(upPath.hops-upHop-1)){
                upPtr+=mrkPtr->blkSize;
                mrkPtr = (pcbMarking*)upPtr;
                continue;
            }

        	pOF = (opaqueField *) (e2ePath + offset);
            
			//adding the opaque fields of the up path
			pOF->type = mrkPtr->type;
            pOF->ingressIf = mrkPtr->ingressIf;
            pOF->egressIf = mrkPtr->egressIf;
            pOF->mac = mrkPtr->mac;

			if(i!=(upPath.hops-upHop)) //ADs on the uppath
				pOF->type = (0x00 | (pOF->type & 0x0f));
			else //TDC AD
				pOF->type = (0x20 | (pOF->type & 0x0f));       
            
			#ifdef _SL_DEBUG_GW
			printf("AD%llu =>",mrkPtr->aid);
			#endif
			offset -= OPAQUE_FIELD_SIZE;
            upPtr += mrkPtr->blkSize;
            mrkPtr = (pcbMarking*)upPtr;
        }

		//Down-path
        //1.3 building opaque field list for the down path
		#ifdef _SL_DEBUG_GW
		printf("Down path in order (from TDC)...\n");
		#endif
        offset = ((upHop+2)*OPAQUE_FIELD_SIZE);

    	pTS = (specialOpaqueField*) dwPtr;
		pSO = (specialOpaqueField*) (e2ePath + offset);
		*pSO = *pTS;
		//1.3.1 info must be set by client...
		pSO->info = 0xc0;// indicates normal shortcut through a common AD

		//1.4.then mark a series of the OF of the selected path
		dwPtr += OPAQUE_FIELD_SIZE;
    	mrkPtr = (pcbMarking*) dwPtr;
    	offset += OPAQUE_FIELD_SIZE;

        for(int i=0;i<downPath.hops;i++){
            //ignoring unsued ADs
            if(i<downPath.hops-dwHop-1){
                dwPtr+=mrkPtr->blkSize;
                mrkPtr = (pcbMarking*)dwPtr; 
                continue; 
            }

            //adding the opaque fields of the down path
        	opaqueField * pOF = (opaqueField *) (e2ePath + offset);
			pOF->type = mrkPtr->type;
        	pOF->ingressIf = mrkPtr->ingressIf;
        	pOF->egressIf = mrkPtr->egressIf;
        	pOF->mac = mrkPtr->mac;

			if(i!=(downPath.hops-dwHop)) //ADs on the downpath
				pOF->type = (0x00 | (pOF->type & 0x0f));
			else //TDC AD
				pOF->type = (0x20 | (pOF->type & 0x0f));       

			#ifdef _SL_DEBUG_GW
			printf("AD%llu =>",mrkPtr->aid);
			#endif
            offset += OPAQUE_FIELD_SIZE;
            dwPtr += mrkPtr->blkSize;
            mrkPtr = (pcbMarking*)dwPtr;
        }    
        
		endpath.length = up_length + down_length;
        endpath.opaque_field = new uint8_t[endpath.length];
        
		memcpy(endpath.opaque_field, e2ePath, endpath.length);
		#ifdef _SL_DEBUG_GW
		printf("\nPath construction is done\n");
		#endif
    }

	//2. path through a peering link
    if (end2end_path_type == PATH_TYPE_PEER) { //peer
		#ifdef _SL_DEBUG_GW
		printf("\nConstruct a peering path\n");
		#endif
    	up_length = (upHop+3) * OPAQUE_FIELD_SIZE; //add OFs for a special OF, itself (to TDC) and upstream AD
    	down_length = (dwHop+3) * OPAQUE_FIELD_SIZE; //add OFs for a special OF, peer AD (to TDC) and upstream AD
		uint8_t e2ePath[(up_length+down_length)*OPAQUE_FIELD_SIZE];
        
		//2.1 Up-path
		//SLC: very long routine...
		//This should be split into several parts?
		//2.1.1 first marking is the special OF
    	specialOpaqueField * pTS = (specialOpaqueField*) upPtr;
		specialOpaqueField * pSO = (specialOpaqueField*) e2ePath;
		*pSO = *pTS;
		//2.1.2 info must be set by client...
		pSO->info = 0xf0;// indicates a shortcut through a peering link
		
		uint16_t peer_up_path_ingressIf;// for down-path peer matching
        uint16_t peer_down_path_ingressIf;

        upPtr += OPAQUE_FIELD_SIZE;
        mrkPtr = (pcbMarking*)upPtr;
        //building opaque field list for the up path
        uint16_t offset = (upHop+2)*OPAQUE_FIELD_SIZE;
		opaqueField * pOF;
		#ifdef _SL_DEBUG_GW
		printf("Up path in reverse order (from TDC)...\n");
		#endif

        for(int i=0;i<upPath.hops;i++){
            //ignore the unused hops
            if(i<(upPath.hops-upHop-1)){
                upPtr+=mrkPtr->blkSize;
                mrkPtr = (pcbMarking*)upPtr;
                continue;
            }
            //adding peering link opaque field
            else if(i==(upPath.hops-upHop)){
                peerMarking* peerPtr = (peerMarking*)(upPtr+PCB_MARKING_SIZE);
                uint16_t num_peers = (mrkPtr->blkSize-PCB_MARKING_SIZE)/PEER_MARKING_SIZE;
                for(int j=0;j<num_peers;j++){
                    if(peerPtr->aid == dwAD) { // take the first that meets the need
						#ifdef _SL_DEBUG_GW
						printf("Finding peer in UP: ingress IF = %d, egress IF = %d\n", 
							peerPtr->ingressIf, peerPtr->egressIf);
						#endif
                        //add peer link
                        pOF = (opaqueField *)(e2ePath+offset);
						pOF->type = mrkPtr->type;
                        pOF->ingressIf = peerPtr->ingressIf;
                        pOF->egressIf = mrkPtr->egressIf;
                        pOF->mac = peerPtr->mac;
                        peer_up_path_ingressIf = peerPtr->ingressIf; //mark peer interfaces for down-path peer OF searching
                        peer_down_path_ingressIf = peerPtr->egressIf;
                        offset -= OPAQUE_FIELD_SIZE;
                        break;
                    }
                    peerPtr++;
                }
            }
        	
			pOF = (opaqueField *) (e2ePath + offset);
            
			//adding the opaque fields of the up path
			pOF->type = mrkPtr->type;
            pOF->ingressIf = mrkPtr->ingressIf;
            pOF->egressIf = mrkPtr->egressIf;
            pOF->mac = mrkPtr->mac;

			if(i!=(upPath.hops-upHop)) //ADs on the uppath
				pOF->type = (0x00 | (pOF->type & 0x0f));
			else //TDC AD
				pOF->type = (0x20 | (pOF->type & 0x0f));       
            
			#ifdef _SL_DEBUG_GW
			printf("AD%llu =>",mrkPtr->aid);
			#endif
			offset -= OPAQUE_FIELD_SIZE;
            upPtr += mrkPtr->blkSize;
            mrkPtr = (pcbMarking*)upPtr;
        }

		//2.3 Downpath
        //adding down path opaque field 
		#ifdef _SL_DEBUG_GW
		printf("Down path in order (from TDC)...\n");
		#endif
        offset = ((upHop+3)*OPAQUE_FIELD_SIZE);
    	
		pTS = (specialOpaqueField*) dwPtr;
		pSO = (specialOpaqueField*) (e2ePath + offset);
		*pSO = *pTS;
		//2.3.1 info must be set by client...
		pSO->info = 0xf0;// indicates a shortcut through a peering link

		//2.3.2 then mark a series of the OF of the selected path
		dwPtr += OPAQUE_FIELD_SIZE;
    	mrkPtr = (pcbMarking*) dwPtr;
    	offset += OPAQUE_FIELD_SIZE;

        for(int i=0;i<downPath.hops;++i){
            //ignore unused hops
            if(i<(downPath.hops-dwHop-1)){
                 dwPtr+=mrkPtr->blkSize;
                 mrkPtr = (pcbMarking*)dwPtr;
                 continue;
            }
            //add peering link opaque filed
            else if(i==(downPath.hops-dwHop)) {
                  peerMarking* peerPtr = (peerMarking*)(dwPtr+PCB_MARKING_SIZE);
                  uint16_t num_peer = (mrkPtr->blkSize-PCB_MARKING_SIZE)/PEER_MARKING_SIZE;
                  for(int j=0;j<num_peer;j++) {
						#ifdef _SL_DEBUG_GW
						printf("Finding peer in DN: ingress IF = %d, egress IF = %d\n", 
							peerPtr->ingressIf, peerPtr->egressIf);
						#endif
                      if(peerPtr->aid==upAD && peerPtr->ingressIf==peer_down_path_ingressIf
                                            && peerPtr->egressIf==peer_up_path_ingressIf){ //TBD-exception?
                        	pOF = (opaqueField *)(e2ePath+offset);
							pOF->type = mrkPtr->type;
                        	pOF->ingressIf = peerPtr->ingressIf;
                        	pOF->egressIf = mrkPtr->egressIf;
                        	pOF->mac = peerPtr->mac;
                          	offset+=OPAQUE_FIELD_SIZE;
							#ifdef _SL_DEBUG_GW
							printf("Peer info: ingress IF = %d, egress IF = %d\n", 
								pOF->ingressIf, pOF->egressIf);
							#endif
                          	break;
                      }
                      peerPtr++;
                  }
            }
			
			pOF = (opaqueField *) (e2ePath + offset);
            
			//adding the opaque fields of the up path
			pOF->type = mrkPtr->type;
            pOF->ingressIf = mrkPtr->ingressIf;
            pOF->egressIf = mrkPtr->egressIf;
            pOF->mac = mrkPtr->mac;

			if(i!=(downPath.hops-dwHop)) //ADs on the uppath
				pOF->type = (0x00 | (pOF->type & 0x0f));
			else //TDC AD
				pOF->type = (0x20 | (pOF->type & 0x0f));       
            
			#ifdef _SL_DEBUG_GW
			printf("AD%llu =>",mrkPtr->aid);
			#endif
			offset += OPAQUE_FIELD_SIZE;
            dwPtr += mrkPtr->blkSize;
            mrkPtr = (pcbMarking*)dwPtr;
        }
		endpath.length = up_length + down_length;
        endpath.opaque_field = new uint8_t[endpath.length];
        
		memcpy(endpath.opaque_field, e2ePath, endpath.length);
		#ifdef _SL_DEBUG_GW
		printf("Path construction is done\n");
		#endif
    }
}


void 
SCIONPathInfo::getCorePath(halfPath& down_path, halfPath& up_path, fullPath& endpath){

    //variables for path building
    uint8_t* dwPtr = down_path.path_marking;
    uint8_t* upPtr = up_path.path_marking;
    uint8_t dwHop = down_path.hops;
    uint8_t upHop = up_path.hops;

    uint16_t up_length = (upHop+1) * OPAQUE_FIELD_SIZE; //add a special OF, i.e., up timestamp
    uint16_t down_length = (dwHop+1) * OPAQUE_FIELD_SIZE; //add a special OF, i.e., down timestamp

    uint8_t e2ePath[ up_length + down_length ];
    uint8_t up[ up_length ];
    uint8_t down[ down_length ];

    
    memset(up, 0, up_length);
    memset(down, 0, down_length);

	//Up-path
	//1. first marking is the special OF
    specialOpaqueField * pTS = (specialOpaqueField*) upPtr;
	specialOpaqueField * pSO = (specialOpaqueField*) e2ePath;
	*pSO = *pTS;
	//1.1 info must be set by client...
	pSO->info = 0x80;

	//2. then mark a series of the OF of the selected path
	upPtr += OPAQUE_FIELD_SIZE;
    pcbMarking * mrkPtr = (pcbMarking*) upPtr;
    uint16_t offset = upHop * OPAQUE_FIELD_SIZE;

    //get opaque field for the up path and put them into the new packet 
	//SL: add special opaque field first...

	#ifdef _SL_DEBUG_GW
	printf("End-to-end Path:\n");
	printf("[Up-path] ");
	#endif
    for (int i = 0; i < upHop; i++){
        opaqueField * pOF = (opaqueField *) (e2ePath + offset);

		//SLT: Need to be changed --> Just copy the opaque field.
		pOF->type = mrkPtr->type;
        pOF->ingressIf = mrkPtr->ingressIf;
        pOF->egressIf = mrkPtr->egressIf;
		//pOF->exp = mrkPtr->exp;
        pOF->mac = mrkPtr->mac;
		#ifdef _SL_DEBUG_GW
		printf("(%d | %d) => ",pOF->ingressIf, pOF->egressIf);
		#endif
 		//SL: type should be set with a function....
		
		if(i) //ADs on the uppath
			pOF->type = (0x00 | (pOF->type & 0x0f));
		else //TDC AD
			pOF->type = (0x20 | (pOF->type & 0x0f));       

        offset -= OPAQUE_FIELD_SIZE;
        upPtr += mrkPtr->blkSize;
        mrkPtr = (pcbMarking*) upPtr;
    }

    offset = ((upHop+1) * OPAQUE_FIELD_SIZE);//offset
    
	//Down-path
   	//3. first marking is the special OF
    pTS = (specialOpaqueField*) dwPtr;
	pSO = (specialOpaqueField*) (e2ePath + offset);
	*pSO = *pTS;
	//3.1 info must be set by client...
	pSO->info = 0x80; //indicates TDC path

	//4. then mark a series of the OF of the selected path
	//dwPtr += OPAQUE_FIELD_SIZE *2;
	dwPtr += OPAQUE_FIELD_SIZE;
    mrkPtr = (pcbMarking*) dwPtr;
    offset += OPAQUE_FIELD_SIZE;


    //get opaque field for the down path and put them into the new packet
    mrkPtr = (pcbMarking*)dwPtr;
	#ifdef _SL_DEBUG_GW
	printf("\n[Down-path] ");
	#endif
    for (int i = 0; i < dwHop; i++){
        opaqueField * pOF = (opaqueField *) (e2ePath + offset);
		pOF->type = mrkPtr->type;
        pOF->ingressIf = mrkPtr->ingressIf;
        pOF->egressIf = mrkPtr->egressIf;
		//pOF->exp = mrkPtr->exp;
        pOF->mac = mrkPtr->mac;
		#ifdef _SL_DEBUG_GW
		printf("(%d | %d) => ",pOF->ingressIf, pOF->egressIf);
		#endif
		//SL: type should be set with a function....
		if(i) //ADs on the downpath
			pOF->type = (0x00 | (pOF->type & 0x0f));
		else //TDC AD
			pOF->type = (0x20 | (pOF->type & 0x0f));       

        offset += OPAQUE_FIELD_SIZE;
        dwPtr += mrkPtr->blkSize;
        mrkPtr = (pcbMarking*) dwPtr;       
	}
	#ifdef _SL_DEBUG_GW
	printf(" : end\n");
	#endif

    endpath.length = up_length + down_length;
    endpath.opaque_field = new uint8_t[endpath.length];
    
    memcpy(endpath.opaque_field, e2ePath, endpath.length);

}

/*SL: if the first arg is addr, 
storeOpaqueField stores OF for reverse path construction*/
bool 
SCIONPathInfo::storeOpaqueField(HostAddr &addr, fullPath &of) {

	//1. reverse the opaque fields to construct a reverse path
	if(reverseOpaqueField(of) == SCION_FAILURE) {
		#if SL_DEBUG_GW
		printf("Failure in reverting Opaque Field\n");
		#endif
		return SCION_FAILURE;
	}

	//2. store it for later use
	std::map<HostAddr, std::list<fullPath> >::iterator itr;

	if((itr = m_inOF.find(addr)) == m_inOF.end()) {
		list<fullPath> v;
		v.push_back(of);
		m_inOF.insert(pair<HostAddr,list<fullPath> >(addr,v));
	} else {
		//if the size of the list exceeds the threshold, remove the oldest one
		if(itr->second.size() >= MAX_HOST_OF_STORE) {
			delete itr->second.front().opaque_field;
			itr->second.pop_front();
		}
		itr->second.push_back(of);
	}

	return SCION_SUCCESS;
}

/*SL: if the first arg is addr, 
storeOpaqueField stores the OF of a resolved path (to destination AD)*/
bool 
SCIONPathInfo::storeOpaqueField(uint64_t &adaid, fullPath &of) {
	return SCION_SUCCESS;
}

/*SL: turn around the provided opaque field to construct a return path */
bool
SCIONPathInfo::reverseOpaqueField(fullPath &path) {
	uint8_t buf[path.length];
	uint8_t hops = path.length / OPAQUE_FIELD_SIZE; //path.length is stored in Bytes
	uint8_t* p;

	uint64_t ts = *(uint64_t *)path.opaque_field;
	//uint64_t * ts = reinterpret_cast<uint64_t *>(path.opaque_field);
	for(int i=1; i<hops; i++) {
		p = path.opaque_field+path.length-i*OPAQUE_FIELD_SIZE;
		if(*p &SPECIAL_OF) {//i.e., special Opaque Field
			//1. copy this TS opaque field at the front
			*(uint64_t *)buf = *(uint64_t *)p;
			//memcpy(buf,p, OPAQUE_FIELD_SIZE);
			//2. copy the original up-path TS to here
			*(uint64_t *)(buf+i*OPAQUE_FIELD_SIZE) = ts;
			//memcpy(buf+i*OPAQUE_FIELD_SIZE,ts, OPAQUE_FIELD_SIZE);
		} else { 
			*(uint64_t *)(buf+i*OPAQUE_FIELD_SIZE) = *(uint64_t*)p;
		}
	}

	//Now, replace the original opaque field with the buf (i.e., that of the reverse path)
	memcpy(path.opaque_field, buf, path.length);

	return SCION_SUCCESS;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SCIONPathInfo)
ELEMENT_MT_SAFE(SCIONPathInfo)

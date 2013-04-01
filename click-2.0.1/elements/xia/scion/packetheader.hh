#ifndef SCIONPACKETHEADER_HH_INCLUDED
#define SCIONPACKETHEADER_HH_INCLUDED

#include <stdint.h>
#include <memory.h>
#include <map>
#include <vector>
#include "define.hh"
#include <string>

#pragma pack(push)
#pragma pack(1)
using namespace std;

enum PathType{
    Core = 0,
    CrossOver,
    PeerLink,
};


enum PacketType{
    DATA=0,			//data packet 
    AID_REQ,		//Address request to local elements (from SCIONSwitch)
    AID_REP,		//AID reply to switch
    ROT_REQ,		//Root of Trust file request to parent AD
    ROT_REP,		//Root of Trust file reply from parent AD
	TO_LOCAL_ADDR=100,	//threshold
    BEACON=101,			//PCB
    CERT_REQ_LOCAL,	//local certificate request (to certificate server)
    CERT_REP_LOCAL,  	//local certificate reply (from certificate server)
    CERT_REQ,       //Certificate Request to parent AD
    CERT_REP,		//Certificate Reply from parent AD
    PATH_REQ_LOCAL,		//path request to local path server
    PATH_REP_LOCAL, 	//path reply from local path server
    PATH_REQ,		//Path request to TDC
    PATH_REP, 		//Path reply from TDC
    PATH_REG,		//Path registration to TDC
    UP_PATH,        //up-path to TDC (beacon server -> path server)
    ROT_REQ_LOCAL, //105//Root of Trust file reply to local certificate server
    ROT_REP_LOCAL, 		//Root of Trust file reply from local certificate server	
    OFG_KEY_REQ,		//opaque field generation key request to CS
    OFG_KEY_REP,		//opaque field generation key reply from CS 
    IFID_REQ,		//interface ID request to the peer router (of the neighbor AD)
    IFID_REP, 		//interface ID reply from the peer router
};

enum ReturnValues{
    SCION_SUCCESS=0,
    SCION_FAILURE,
};

enum SignatureType{
    SIZE_128=0,
    SIZE_256,
    SIZE_384,
};

enum IDSize{
	SIZE_TID=4,
	SIZE_AID=8,
};

struct scionCommonHeader{
    uint8_t type;
    uint8_t hdrLen;
    uint16_t totalLen;
    uint8_t timestamp;
    uint8_t srcLen;
    uint8_t dstLen;
    uint8_t flag;
    uint8_t currOF;
    uint8_t numOF;
    uint8_t l4Proto;
    uint8_t nRetCap;
    uint8_t capReqInfo;
    uint8_t newCap;
    uint8_t pathVal;
    uint8_t srcAuth;
};

//SL: address type
enum ADDR_TYPE{
	HOST_ADDR_SCION = 0,
	HOST_ADDR_IPV4,
	HOST_ADDR_IPV6,
	HOST_ADDR_AIP,
};

//SL: opaque field type
enum OF_TYPE{
	TDC_PATH = 0,
	PEER_PATH,
};

struct pcbMarking{
    uint64_t aid;
    uint32_t certID;
    uint16_t sigLen;
    uint16_t blkSize;
    uint8_t type; //used 2 LSB for exp
    //uint8_t exp : 2;
    uint16_t ingressIf;
    uint16_t egressIf;
    uint32_t mac : 24;
    uint16_t tdid;
    uint16_t bwAlloc;
    uint8_t CLS : 1;
    uint32_t reserved : 31;
};

struct peerMarking{
    uint64_t aid;
    uint8_t type;
    //uint8_t exp : 4;
    uint16_t ingressIf; //SL: ingress IF to the border router 
    uint16_t egressIf; //SL: egress IF from the peering router
    uint32_t mac : 24;
    uint16_t tdid;
    uint16_t bwAlloc;
    uint8_t CLS : 1;
    uint32_t reserved : 31;
};


struct pathHop{
    uint16_t ingressIf;
    uint16_t egressIf;
    uint32_t mac : 24;
};

//SL: pathHop needs to be removed
struct opaqueField{
    opaqueField(uint8_t t, uint16_t ing, uint16_t eg, uint8_t e, uint32_t m):
	type(t),ingressIf(ing),egressIf(eg),mac(m)
	{
    }
    opaqueField():
	type(0),ingressIf(0),egressIf(0),mac(0)
	{
    }
      
    uint8_t type;//first 2bits are type, and last 2bits are expiration time
    uint16_t ingressIf : 16;
    uint16_t egressIf : 16;
    uint32_t mac : 24;
};

//SL: for the special Opaque Field
struct specialOpaqueField{
    uint8_t info;
    uint32_t timestamp;
    uint16_t tdid;
    uint8_t hops;
};

struct specialROTField{
    uint8_t info;
    uint32_t rot_version;
    uint16_t ifid;
    uint8_t reserved;
};


//SL: host address
/**
	endhost address
*/
class HostAddr{
	private:
		uint8_t m_iLength;		//address length 
		uint8_t m_iType; 		//SCION (8B), IPv4(4B), IPv6(16B), or AIP(20B); 
								//currently supports 4 address types
		uint8_t m_Addr[MAX_HOST_ADDR_SIZE];	//multiple of 4B
	public:
		HostAddr():m_iType(0),m_iLength(0) {}
		HostAddr(uint8_t type, uint64_t addr) {
			setAddrType(type);
			memcpy(m_Addr, &addr, SCION_ADDR_SIZE);
		}		
		
		HostAddr(uint8_t type, uint32_t addr) {
			setAddrType(type);
			memcpy(m_Addr, &addr, IPV4_SIZE);
		}

		HostAddr(uint8_t type, uint8_t * addr) {
			setAddrType(type);
			int size;
			if(type == HOST_ADDR_IPV6)
				size = IPV6_SIZE;
			else
				size = MAX_HOST_ADDR_SIZE; //including AIP
			memcpy(m_Addr, &addr, size);
		}

		~HostAddr() {}

		HostAddr& operator=( const HostAddr& rhs ) {
			m_iType = rhs.m_iType;
			m_iLength = rhs.m_iLength;
			memcpy(m_Addr, rhs.m_Addr, m_iLength);
			return *this;
		}

		bool operator==( const HostAddr& rhs) {
			if((m_iType != rhs.m_iType) ||(m_iLength != rhs.m_iLength))
				return false;
			
			if(memcmp(m_Addr, rhs.m_Addr, m_iLength))
				return false;
			else
				return true;
		}

		bool operator<( const HostAddr& rhs) const{
			//if lhs has a short address length than rhs
			//lhs address is assumed "small"
			if(m_iLength < rhs.m_iLength)
				return true;

			for(int i=0;i<rhs.m_iLength;i++){
				if(m_Addr[i] < rhs.m_Addr[i])
					return true;
                else if(m_Addr[i] > rhs.m_Addr[i])
                    return false;
			}
			return false;
		}

		
		uint8_t getLength() {return m_iLength;}
        uint8_t getType() {return m_iType;}
		void getIPv4Addr(uint8_t * addr) {memcpy(addr, m_Addr, IPV4_SIZE);} 
		void getIPv6Addr(uint8_t * addr) {memcpy(addr, m_Addr, IPV6_SIZE);} 
		void getAIPAddr(uint8_t * addr) {memcpy(addr, m_Addr, AIP_SIZE);} 
		void getSCIONAddr(uint8_t * addr) {memcpy(addr, m_Addr, SCION_ADDR_SIZE);} 
		void setIPv4Addr(uint32_t addr) {setAddrType(HOST_ADDR_IPV4); memcpy(m_Addr, &addr, IPV4_SIZE);} 
		void setIPv6Addr(uint8_t * addr) {setAddrType(HOST_ADDR_IPV6); memcpy(m_Addr, addr, IPV6_SIZE);} 
		void setAIPAddr(uint8_t * addr) {setAddrType(HOST_ADDR_AIP); memcpy(m_Addr, addr, AIP_SIZE);} 
		void setSCIONAddr(uint64_t addr) {setAddrType(HOST_ADDR_SCION); memcpy(m_Addr, &addr, SCION_ADDR_SIZE);} 
		uint8_t *getAddr() {return m_Addr;}

		void setAddrType(uint8_t type) {
			m_iType = type;
			switch(type) {
			case HOST_ADDR_SCION:
				m_iLength = SCION_ADDR_SIZE;
				break;
			case HOST_ADDR_IPV4:
				m_iLength = IPV4_SIZE;
				break;
			case HOST_ADDR_IPV6:
				m_iLength = IPV6_SIZE;
				break;
			case HOST_ADDR_AIP:
				m_iLength = AIP_SIZE;
				break;
			default: break;
			}
		}
        
        void setAddr(uint8_t type, uint8_t* addr){
			switch(type) {
			case HOST_ADDR_SCION:
			    memcpy(m_Addr, addr, SCION_ADDR_SIZE);	
                break;
			case HOST_ADDR_IPV4:
			    memcpy(m_Addr, addr, IPV4_SIZE);	
			case HOST_ADDR_IPV6:
			    memcpy(m_Addr, addr, IPV6_SIZE);	
				break;
			case HOST_ADDR_AIP:
			    memcpy(m_Addr, addr, AIP_SIZE);	
				break;
			default: break;
			}
        }


		uint64_t numAddr() { //This is just for debugging purpose and for IPV4 and SCION Addresses
			uint64_t addr;
			switch(m_iType) {
			case HOST_ADDR_IPV4:
				addr = (uint64_t) *((uint32_t *)m_Addr);
			break;
			case HOST_ADDR_SCION:
				addr = *((uint64_t *)m_Addr);
			break;
			default:
				addr = 0;//can't be represented with 64bit number.
			break;
			}
			return addr;
		}
};

/*
	SL:
	SCION header structure
*/
class scionHeader{
	public:
		scionHeader():p_of(NULL),n_of(0){
			memset(&cmn,0,sizeof(scionCommonHeader));
		}
		~scionHeader(){
			//memory release should be done where it's allocated
			//if(p_of)
			//	delete p_of;
		}
	
		scionCommonHeader cmn; 	//common header
		HostAddr src;			//source address
		HostAddr dst;			//destination address
		uint8_t * p_of;		//opaque field
		uint8_t n_of;			//number of opaque fields
};


/*pathInfo structure that comes first
in the PATH_REP packet
*/
struct pathInfo{
    uint64_t target;
    uint32_t tdid;
	//SL: to be removed///////////////
    uint32_t timestamp;
    uint16_t totalLength; //total length of (PCB - sign) in bytes
    uint16_t numHop;
	///////////////////////////////
    uint16_t option;
};

/*SL: Moved here since
Client and Gateway can use these structs
*/
///////////////////////////////////////////////
//Peering link information
struct Peer {
    uint64_t aid;	//own ADAID
    uint64_t naid;	//neighbor ADAID
    uint16_t srcIfid;	//ingress IF
    uint16_t dstIfid;	//egress IF of peer
};

//Half-path information
struct halfPath 
{
    uint16_t hops;
    uint16_t num_peers;
    uint32_t timestamp;
    vector<uint64_t> path;  //vector of ADAID
    std::multimap<uint64_t, Peer> peer;
    uint16_t length; //total length of (PCB - sign) in bytes
    uint8_t* path_marking; 
};

//End-to-end path information
//SL: length and opaque fields are necessary for the new packet format
//This would be used in SCIONPathInfo 
//1) to cache opaque fields of a resolved path
//2) to construct reverse path to the sender
struct fullPath {
    //uint32_t up_timestamp;
    //uint32_t down_timestamp;
    //uint16_t hops;
    //uint16_t crossover; //SL: added by Chang
    //uint16_t interface; //SL: added by Chang
	uint8_t sent; //SL: used only by client... this should be removed
	uint8_t length; //length of OFs in Byte (max. 32 opaque fields)
	time_t cache_time; //time that this opaque fields are cached.
    uint8_t* opaque_field;
};

///////////////////////////////////////////////

struct certInfo{
    uint64_t target;
    uint32_t tdid;
    uint16_t rotVersion;
    uint16_t length;
};

struct certReq{
    int numTargets;
    uint64_t targets[MAX_TARGET_NUM];
};

struct certRep{
    int numCerts;

};

class SCIONPacketHeader{

    public:

        static void initPacket(uint8_t* pkt, uint16_t type, uint16_t totalLength, uint64_t src,
                uint64_t dst, uint32_t upTs, uint32_t dwTs, uint8_t hops,uint16_t
                interface);

        static uint8_t getType(uint8_t* pkt);        
        static uint8_t getHdrLen(uint8_t* pkt);
        static uint16_t getTotalLen(uint8_t* pkt);
        static uint8_t getTimestampPtr(uint8_t* pkt);
        static uint8_t getTimestampInfo(uint8_t* pkt, uint8_t pTS);
        static uint32_t getTimestamp(uint8_t* pkt);
        static uint8_t getSrcLen(uint8_t* pkt);
        static uint8_t getDstLen(uint8_t* pkt);
        static uint8_t getFlags(uint8_t* pkt);
        static uint8_t* getCurrOF(uint8_t* pkt);
        static uint8_t* getOF(uint8_t* pkt, int offset);
        static uint8_t getOFType(uint8_t* pkt);
        static uint8_t getNumOF(uint8_t* pkt);
        static uint8_t getL4Proto(uint8_t* pkt);
        static uint8_t getNRetCap(uint8_t* pkt);
        static uint8_t getCapReqInfo(uint8_t* pkt);
        static uint8_t getNewCap(uint8_t* pkt);
        static uint8_t getPathVal(uint8_t* pkt);
        static uint8_t getSrcAuth(uint8_t* pkt);

        static void getSrcAddr(uint8_t* pkt, uint8_t** srcAddr);
        static HostAddr getSrcAddr(uint8_t* pkt);
        static void getDstAddr(uint8_t* pkt, uint8_t** dstAddr);
        static HostAddr getDstAddr(uint8_t* pkt);
        static void setType(uint8_t* pkt, uint8_t type);
        static void setHdrLen(uint8_t* pkt, uint8_t hdrLen);
        //SLT:
		static void adjustPacketLen(uint8_t* pkt, int offset);
        static void setTotalLen(uint8_t* pkt, uint16_t totalLen);
        static void setTimestampPtr(uint8_t* pkt, uint8_t tsPtr);
        static void setTimestampOF(uint8_t* pkt, specialOpaqueField& ts);
        static void setSrcLen(uint8_t* pkt, uint8_t srcLen);
        static void setDstLen(uint8_t* pkt, uint8_t dstLen);
        static void setFlags(uint8_t* pkt, uint8_t totalLen);
        static void setCurrOFPtr(uint8_t* pkt, uint8_t currOFPtr);
        static void setNumOF(uint8_t* pkt, uint8_t numOF);
        static void setL4Proto(uint8_t* pkt, uint8_t l4Proto);
        static void setNRetCap(uint8_t* pkt, uint8_t nRetCap);
        static void setCapReqInfo(uint8_t* pkt, uint8_t capReqInfo);
        static void setNewCapPtr(uint8_t* pkt, uint8_t newCap);
        static void setPathValPtr(uint8_t* pkt, uint8_t pathVal);
        static void setSrcAuthPtr(uint8_t* pkt, uint8_t srcAuth);
        static void addTotalLen(uint8_t* pkt, uint16_t len);
      
        static void putSpecialOpaque(uint8_t* pkt, uint8_t info, uint32_t
        timestamp, uint16_t tdid, uint8_t numHop, uint8_t offset);
     
       	//SLT:
        static void addSrcAddr(uint8_t* pkt, HostAddr srcAddr);
        static void addDstAddr(uint8_t* pkt, HostAddr dstAddr);
        
        static void clearSrcAddr(uint8_t* pkt);
        static void clearDstAddr(uint8_t* pkt);
        
        static void removeSrcAddr(uint8_t* pkt);
        static void removeDstAddr(uint8_t* pkt);
        
        static void setSrcAddr(uint8_t* pkt, HostAddr srcAddr);
        static void setDstAddr(uint8_t* pkt, HostAddr dstAddr);
        
        static void setPath(uint8_t* pkt, uint8_t* path, 
        uint8_t pathLen,uint8_t numOF);
        
        static void addNumOF(uint8_t* pkt, uint8_t numOF);

		//SL: added for the new packet format
		static uint8_t getCurrOFPtr(uint8_t * pkt); // Current Opaque Field Pointer
		static uint8_t getCurrentTimestampPtr(uint8_t * pkt); //current Timestamp *
		static uint32_t getTimestamp(uint8_t * pkt, uint8_t ts_ptr); //timestamp
		static uint8_t isRegularOF(uint8_t * pkt, uint8_t pCurrOF); //regular OF?

	    static uint8_t getOFPtr(uint8_t* pkt);	
		//increase OF pointer by cnt OFs, and return the current OF pointer
		static uint8_t increaseOFPtr(uint8_t * pkt, uint8_t cnt); 
		static void setUppathFlag(uint8_t* pkt);
		static void setDownpathFlag(uint8_t* pkt);
		static uint16_t getEgressInterface(uint8_t* pkt); //returns the egress interface id in a PCB
		static uint16_t getIngressInterface(uint8_t* pkt); //returns the ingress interface id in a PCB
		static uint16_t getOutgoingInterface(uint8_t* pkt); //returns the outgoing interface id in the current forwarding path (differ whether a packet is on the uppath or the downpath)

		static uint8_t setHeader(uint8_t * packet, scionHeader & hdr); //set scionHeader. This simplifies packet construction
    
    private:
};

#pragma pack(pop)

#endif

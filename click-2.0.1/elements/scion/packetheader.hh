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

#ifndef CLICK_2_0_1_ELEMENTS_SCION_PACKETHEADER_H_
#define CLICK_2_0_1_ELEMENTS_SCION_PACKETHEADER_H_

#include "define.hh"
#include <stdint.h>
#include <memory.h>
#include <map>
#include <vector>
#include <string>

#pragma pack(push)
#pragma pack(1)
using namespace std;

/**
    @file packetheader.hh
    @class SCIONPacketHeader
    @enum PathType
    @brief Defines the type of the path
*/
enum PathType {
  Core = 0, /** Path to the core */
  CrossOver, /** Path with cross over */
  PeerLink, /** Path with peer link */
};

/**
    @brief Packet Type
*/
enum PacketType {
  DATA = 0,     /** data packet */
  AID_REQ,    /** Address request to local elements (from SCIONSwitch) */
  AID_REP,    /** AID reply to switch */
  TO_LOCAL_ADDR = 100,  /** Threshold to distinguish local control packets */
  BEACON = 101,     /** PCB type */
  CERT_REQ_LOCAL, /**local certificate request (to certificate server) */
  CERT_REP_LOCAL,   /**local certificate reply (from certificate server) */
  CERT_REQ,       /**Certificate Request to parent AD */
  CERT_REP,   /**Certificate Reply from parent AD*/
  PATH_REQ_LOCAL,   /**path request to local path server*/
  PATH_REP_LOCAL,   /**path reply from local path server*/
  PATH_REQ,   /**Path request to TDC*/
  PATH_REP,     /**Path reply from TDC*/
  PATH_REG,   /**Path registration to TDC*/
  UP_PATH,        /**up-path to TDC (beacon server -> path server)*/
  ROT_REQ_LOCAL,  /**Root of Trust file reply to local certificate server*/
  ROT_REP_LOCAL,    /**Root of Trust file reply from local certificate server*/
  OFG_KEY_REQ,    /**opaque field generation key request to CS*/
  OFG_KEY_REP,    /**opaque field generation key reply from CS */
  IFID_REQ,   /**interface ID request to the peer router (of the neighbor AD)*/
  IFID_REP,     /**interface ID reply from the peer router*/
  ROT_REQ,    /**Root of Trust file request to parent AD*/
  ROT_REP,    /**Root of Trust file reply from parent AD*/
};

enum ReturnValues {
  SCION_SUCCESS = 0,
  SCION_FAILURE,
};

enum SignatureType {
  SIZE_128 = 0,
  SIZE_256,
  SIZE_384,
};

enum IDSize {
  SIZE_TID = 4,
  SIZE_AID = 8,
};

//SL: address type
enum ADDR_TYPE {
  ADDR_NA = 0,
  HOST_ADDR_SCION,
  HOST_ADDR_IPV4,
  HOST_ADDR_IPV6,
  HOST_ADDR_AIP,
};

//SL: opaque field type
enum OF_TYPE {
  TDC_PATH = 0,
  PEER_PATH,
};

/**
    @class SCIONPacketHeader
    @struct scionCommonHeader
    @brief Defines the 16byte Common header of the SCION packet

    This struct will represent the 16Byte Common Header for SCION Packet. This
    header will be included and should be included to ALL SCION packets.
*/
struct scionCommonHeader {
  /** Packet Type of the packet*/
  uint8_t type;
  /** Header length that includes the path */
  uint8_t hdrLen;
  /** Total Length of the packet */
  uint16_t totalLen;
  /** Offset inside the packet to the timestamp */
  uint8_t timestamp;
  /** Length of the source address */
  uint8_t srcLen;
  /** Length of the destination address*/
  uint8_t dstLen;
  /** Flag field */
  uint8_t flag;
  /** Index of current opaque field*/
  uint8_t currOF;
  /** Number of opaque field in the packet*/
  uint8_t numOF;
  /** Layer 4 protocl type */
  uint8_t l4Proto;
  /** Number of Returned Capabilities */
  uint8_t nRetCap;
  uint8_t capReqInfo;
  uint8_t newCap;
  uint8_t pathVal;
  uint8_t srcAuth;
};


/**
    @struct pcbMarking
    @brief Defines the pcb marking
    This structure is used in the beacon server. This struct represents
    the information that beacon server puts in to the beacon.
*/
struct pcbMarking {
  /** AID of the current AD */
  uint64_t aid;
  /** Certificate ID of the current AD */
  uint32_t certID;
  /** Signature Length */
  uint16_t sigLen;
  /** The size of the current Marking */
  uint16_t blkSize;
  /** Type field */
  uint8_t type; //used 2 LSB for exp
  //uint8_t exp : 2;
  /** Ingress Interface ID */
  uint16_t ingressIf;
  /** Egress Interface ID */
  uint16_t egressIf;
  /** MAC value */
  uint32_t mac : 24;
  /** TD ID */
  uint16_t tdid;
  uint16_t bwAlloc;
  uint8_t CLS : 1;
  uint32_t reserved : 31;
};

/**
    @struct peerMarking
    @brief Defines the special Marking for peering operation.
    Beacon server uses this struct to put peering information to the beacon.
*/
struct peerMarking {
  /** AID of the 'peer' */
  uint64_t aid;
  /** type of the marking */
  uint8_t type;

  //uint8_t exp : 4;
  /** Interface ID that connects to the peering AD */
  uint16_t ingressIf; //SL: ingress IF to the border router
  /** Interface ID that the 'peer' uses for this link */
  uint16_t egressIf; //SL: egress IF from the peering router
  /** MAC value */
  uint32_t mac : 24;
  /** TD ID */
  uint16_t tdid;
  uint16_t bwAlloc;
  uint8_t CLS : 1;
  uint32_t reserved : 31;
};

//SL: pathHop needs to be removed
/**
    @struct pathHop
    @brief Defines a hop for the SCION path
    This struct represents a hop in the scion path. The list of pathHop will be
    a path.
*/
struct pathHop {
  /** Ingress interface */
  uint16_t ingressIf;
  /** Egress interface */
  uint16_t egressIf;
  /** MAC value */
  uint32_t mac : 24;
};

/**
    @struct opaqueField
    @brief Opaque Field structure
    This structure defines opaque field.
*/
struct opaqueField {
  opaqueField(uint8_t t, uint16_t ing, uint16_t eg, uint8_t e, uint32_t m)
      : type(t), ingressIf(ing), egressIf(eg), mac(m) {
    // Nothing to do.
  }

  opaqueField() : type(0), ingressIf(0), egressIf(0), mac(0) {
    // Nothing to do.
  }

  /**
      @brief Type of the opaque field
      Types are used in many way especially for defining normal opaque field and
      special opaque field in the forwarding operation.
      First 2 bits are type and last 2 bits are expiration time.
  */
  uint8_t type;
  /**
      @brief Ingress Interface Field.
      Ingress Interface field.
  */
  uint16_t ingressIf : 16;
  /**
      @brief Egress Interface Field.
      Egress Interface field.
  */
  uint16_t egressIf : 16;
  /**
      @brief MAC
      MAC
  */
  uint32_t mac : 24;
};

/**
    @brief Special Opaque Field Structure
    This structure represents the special opaque field that contains the
    time stamp.
*/
struct specialOpaqueField {
  /** Info field with timestamp information */
  uint8_t info;
  /** Timestamp value in 32 bit number */
  uint32_t timestamp;
  /** TD Id */
  uint16_t tdid;
  /** Number of hops under this timestamp (either up or down)*/
  uint8_t hops;
};

/**
    @brief Structure that represents the special ROT field
    This structure represents a special 8 Byte ROT field.
*/
struct specialROTField {
  /** Info field with ROT information */
  uint8_t info;
  /** ROT version Number */
  uint32_t rot_version;
  /** Interface ID */
  uint16_t ifid;
  /** RESERVED */
  uint8_t reserved;
};

/**
  @brief ROT request format
  If CS gets ROT_REQ_LOCAL, it only needs to provide the ROT of the currentVersion to BS
  If CS does not have the currentVersion in its local repository, the CS request ROTs from
  previousVersion to currentVersion, to the upstream AD

  @note: the previous Version may not be currentVersion-1, if an AD rejoin a TD or
  if it has been out of business for a while
*/
struct ROTRequest {
    /** The most rescent ROT version that the requester has in its local repo */
  uint16_t previousVersion;
    /** The current version of ROT */
  uint16_t currentVersion;
};

/**
  @brief IFID request message structure
  A SCION (border) router sends a IFID_REQ to its neighbor router (in the neighbor AD)
  during its initialization. The IFID_REP from its neighbor contains the interface id of the router,
  which will be used to construct a PCB by the Beacon Server. Hence, the router forwards the IFID_REP
  to the Beacon Server.
*/
struct IFIDRequest {
  /** Interface id of the replying router */
  uint16_t reply_id;
  /** Interface id of the requesting router */
  uint16_t request_id;
};

/**
    @brief HostAddr Object Class

    This class is generalized class that stores all types of address that SCION
    supports. The types of addresses include SCION Address, IPv4, IPv6, and AIP.
    The m_iType specifies the type of address and m_iLength specifies the length
    of the address. Also, m_Addr is used as the container that stores different
    types of addresses.

    The types and length of the addresses are subject to change when necessary.

    @note Currently some of the functions assume that the address is SCION
    address. These functions must be modified sooner or later when other protocols
    are integreated with SCION.

*/
class HostAddr {
 private:
  /**
   * Address Length
   */
  uint8_t m_iLength;  //address length
  /**
   * SCION (8B), IPv4(4B), IPv6(16B), or AIP(20B);
   * currently supports 4 address types
   */
  uint8_t m_iType;
  /**
   * Address of the host or server in 4B number
   */
  uint8_t m_Addr[MAX_HOST_ADDR_SIZE];
 public:
  /**
   * @brief Constructor of HostAddr
   *
   * This is the default constructor that sets the type of the address as
   * SCION and sets the length to 0.
   *
   * @note The length 0 here is a bit wierd. This should be changed.
   */
  HostAddr();
  /**
   * @brief Constructor of HostAddr for SCION Address type
   * @param uint8_t type The type of the address to be stored
   * @param uint64_t The address that will be stored.
   *
   * This constructor is for SCION Address object construction. The
   * instantiated object stores SCION Address and type must by
   * HOST_ADDR_SCION.
   *
   * @note It is wierd to have 'type' and address if the type must be
   * HOST_SCION_ADDR. Maybe we could change the address to a byte array and
   * have just one constructor instead.
   */
  HostAddr(uint8_t type, uint64_t addr);

  /**
   * @brief Constructor of HostAddr for IPv4 Address
   * @param uint8_t type The type of the address that will be stored
   * @param uint32_t addr The address that will be stored
   *
   * This constructor is for IPv4 Address obejct construction. The
   * instantiated object stores IPv4 Address and the type parameter should
   * always be HOST_ADDR_IPV4.
   */
  HostAddr(uint8_t type, uint32_t addr);

  /**
   * @brief Constructor of HostAddr for either IPv6 or AIP
   * @param uint8_t type The type parameter that specifies the type of the
   * address.
   * @param uint8_t* addr The address that will be stored in the object
   *
   * This constructor is for both IPv6 and AIP. If the given type is IPv6,
   * then the constructor sets the length as IPV6_SIZE and
   * MAX_HOST_ADDR_SIZE when if the type is any other type.
   */
  HostAddr(uint8_t type, uint8_t * addr);

  ~HostAddr();

  HostAddr& operator=(const HostAddr& rhs);

  bool operator==(const HostAddr& rhs);

  bool operator<(const HostAddr& rhs) const;

  /**
   * @brief Returns the length of the address
   */
  uint8_t getLength();

  /**
   * @brief Returns the type of the address
   */
  uint8_t getType();

  /**
   * @brief get the IPv4 address from the HostAddr structure
   * @param uint8_t* addr The address will be copied to this buffer.
   * @note The parameter 'addr' must have memory allocated before calling
   * this function.
   */
  void getIPv4Addr(uint8_t * addr);

  /**
   * @brief Returns the IP address in unsigned int
   * @note The address should be checked to be ipv4 address
   *       before this function is called.
   */
  uint32_t getIPv4Addr();

  /**
   * @brief get the IPv6 address from the HostAddr structure
   * @param uint8_t* addr The address will be copied to this buffer.
   * @note The parameter 'addr' must have memory allocated before calling
   * this function.
   */
  void getIPv6Addr(uint8_t * addr);

  /**
   * @brief get the AIP address from the HostAddr structure
   * @param uint8_t* addr The address will be copied to this buffer.
   * @note The parameter 'addr' must have memory allocated before calling
   * this function.
   */
  void getAIPAddr(uint8_t * addr);

  /**
   * @brief get the SCION address from the HostAddr structure
   * @param uint8_t* addr The address will be copied to this buffer.
   * @note The parameter 'addr' must have memory allocated before calling
   * this function.
   */
  void getSCIONAddr(uint8_t * addr);

  /**
   * @brief Set the address of the HostAddr to the given IPv4 address
   * @param uint32_t addr The IPv4 address that will be stored in the
   * object.
   *
   * This function sets the address of the object as the given IPv4
   * address. The type is automatically set to HOST_ADDR_IPV4.
   *
   * @note The setAddrType() function sets the length of the address
   * depending on the address type.
   */
  void setIPv4Addr(uint32_t addr);

  /**
   * @brief Set the address of the HostAddr to the given IPv6 address
   * @param uint8_t* addr The buffer that holds the IPv6 address that will
   * be stored in the object.
   *
   * This function sets the address of the object as the given IPv6
   * address. The type is automatically set to HOST_ADDR_IPV6.
   *
   * @note The setAddrType() function sets the length of the address
   * depending on the address type.
   */
  void setIPv6Addr(uint8_t * addr);

  /**
   * @brief Set the address of the HostAddr to the given AIP address
   * @param uint8_t* addr The buffer that holds the AIP address that will
   * be stored in the object.
   *
   * This function sets the address of the object as the given AIP
   * address. The type is automatically set to HOST_ADDR_AIP.
   *
   * @note The setAddrType() function sets the length of the address
   * depending on the address type.
   */
  void setAIPAddr(uint8_t * addr);

  /**
   *   @brief Set the address of the HostAddr to the given SCIOIN address
   *   @param uint64_t addr The SCION address that will be stored in the object.
   *
   *   This function sets the address of the object as the given SCION
   *   address. The type is automatically set to HOST_ADDR_SCION.
   *
   *   @note The setAddrType() function sets the length of the address
   *   depending on the address type.
   */
  void setSCIONAddr(uint64_t addr);

  /**
   * @brief reurn the address of the HostAddr
   * @note TODO This function should be changed if it is used. Returning
   * the pointer will not do the magic.
   */
  uint8_t *getAddr();

  /**
   * @brief Set the type of the address as the given type
   * @param uint8_t type
   *
   * This function sets the type field of the HostAddr object. The type is
   * given as a parameter and the m_iLength is set depending on the type.
   *
   * @note Maybe it should have default length setting for unknown address
   * type.
   */
  void setAddrType(uint8_t type);

  /**
   * @brief Set the Address of the HostAddr as the given address and the
   * given type
   * @param uint8_t type The type of the address that will be stored in the
   * object
   * @param uint8_t* addr The buffer that stores the address that will be
   * stored in the object.
   *
   * This function sets the address field of the HostAddr object as the
   * 'addr'. Also, the m_iType is set to parameter type. This function does
   * not set the length of the address.
   */
  void setAddr(uint8_t type, uint8_t* addr);


  /**
   * @brief Temporary function that returns the SCION Address of the
   * HostAddr
   *
   * This function only supports IPv4 and SCION address. The SCION address
   * and IPv4 address can be represented as a single type and this function
   * returns both address as numbers.
   *
   * If the address is neither SCION address nor IPv4 address, the function
   * returns 0.
   *
   * @note This function may return error code on the default case
   * indicating that the current address type is not supported by this
   * function. This is just for debugging purpose and for IPV4 and SCION
   * Addresses.
   */
  uint64_t numAddr();
};

//SL: currently support IPV4 encapsulation
//this is why addr is defined 4B

/**
    @struct PortInfo
    @brief Port information struct to support IPv4
    @var ifid
    Member 'ifid' defines the interface id that IPv4 uses.
    @var addr
    Member 'addr' contains the IP address of the sender.
    @var to_addr
    Member 'to_addr' contains the address of the receiver.
*/
struct portInfo {
  portInfo();
  /** Interface id of a router corresponding to its (Click) port */
  uint16_t ifid;
  HostAddr addr;
  HostAddr to_addr;
  bool m_bInitialized;
};


/**
    @brief SCION Header class that contains the header information of the SCION
    Packet.

    @note Currently the path validation and source authentication are not
    supported.
*/
class scionHeader {

 public:
  /**
   * @brief Scion Header Constructor
   *
   * Default constructor of scionHeader. This constructor sets the number
   * of opaque field and opaque field as 0 and Null respectively.
   */
  scionHeader();
  /**
          @brief Scion Header Deconstructor
      */
  ~scionHeader();

  /** Common header of the packet */
  scionCommonHeader cmn;
  /** Source address of the packet */
  HostAddr src;
  /** Destination address of the packet */
  HostAddr dst;
  /** Opaque field pointer of the packet */
  uint8_t * p_of;
  /** Number of opaque field contained in this packet */
  uint8_t n_of;
};


/**
  pathInfo structure that comes first in the PATH_REP packet
*/
struct pathInfo {
  /** Target (the owner of the path) */
  uint64_t target;
  /** TDID of the owner of the path */
  uint32_t tdid;
  //SL: to be removed///////////////
  uint32_t timestamp;
  uint16_t totalLength; //total length of (PCB - sign) in bytes
  uint16_t numHop;
  ///////////////////////////////
  /** Reserved Option Field */
  uint16_t option;
};

/*SL: Moved here since
Client and Gateway can use these structs
*/
///////////////////////////////////////////////
/**
    @brief Peering link information
    Structure that represents the peering link information.
*/
struct Peer {
  /** The AD id of the AD that has this peering link (on path) */
  uint64_t aid; //own ADAID
  /** The AD id of the peer that peers with the AD with 'aid' */
  uint64_t naid;  //neighbor ADAID
  /** Ingress interface ID */
  uint16_t srcIfid; //ingress IF
  /** Interface ID of the PEER */
  uint16_t dstIfid; //egress IF of peer
};
/**
    @brief Half-path information
    Half path (up or down) information structure
*/
struct halfPath {
  /** Length of half-path in AD hops */
  uint16_t hops;      //length of half-path in AD hops
  /** Number of Peering links */
  uint16_t num_peers;   //number of peering links included
  /** Timestamp of this path */
  uint32_t timestamp;   //timestamp
  /** List of ADs in this path */
  vector<uint64_t> path;  //vector of ADAID
  /** List of peers in this path */
  std::multimap<uint64_t, Peer> peer; //list of peers
  /** Length of the path in bytes */
  uint16_t length;    //total length of (PCB - signature) in bytes
  /** Raw PCB markings for this path */
  uint8_t* path_marking;  //raw PCB - signature
};

/**
    @brief End-to-end path information
    This would be used in SCIONPathInfo length and opaque fields are necessary
    for the new packet format
    1) to cache opaque fields of a resolved path
    2) to construct reverse path to the sender
*/
struct fullPath {
  /** Sent flag. Used only by the client */
  uint8_t sent;       //SL: used only by client... this should be removed
  /** Length of the OFs in Byte (max. 32 opaque fields)*/
  uint8_t length;     //length of OFs in Byte (max. 32 opaque fields)
  /** Time that this opaque fields are cached.*/
  time_t cache_time;    //time that this opaque fields are cached.
  /** Opaque fields that would be copied to a packet */
  uint8_t* opaque_field;  //opaque fields that would be copied to a packet
};

///////////////////////////////////////////////

/**
    @brief certificate information
*/
struct certInfo {
  /** ADID of the owner*/
  uint64_t target;    //ADID
  /** TD ID of the owner */
  uint32_t tdid;      //TD to which AD belongs
  /** ROT version */
  uint16_t rotVersion;  //ROT version
  /** Length of the certificate */
  uint16_t length;    //length of certificate
};

/**
    @brief List of certificate Requests
*/
struct certReq {
  /** Number of requeted certificates */
  int numTargets;           //number of requested certificates
  /** The list of targets */
  uint64_t targets[MAX_TARGET_NUM]; //the list of ADs
};

struct certRep {
  int numCerts;
};

#ifndef SPH
#define SPH SCIONPacketHeader
#define SPacket uint8_t
#endif

/**
    SCION Packet Header
*/
class SCIONPacketHeader {

 public:
  /**
      @brief Initializes click packet
      @param uint8_t* pkt The buffer will store all the packet information.
      @param uint16_t type The type of the packet.
      @param uint16_t totalLength The total length of the packet.
      @param uint64_t src The source address of the packet.
      @param uint64_t dst The destination address of the packet.
      @param uint32_t upTs The timestamp for the up path.
      @param uint32_t dwTs The timestamp for the down path.
      @param uint8_t hops The number of hops that the path inside this
      packet has.
      @param uint16_t interface The interface ID that will used as egress
      interface inside the first AD.

      This function initializes and creates a click packet with information
      given as parameters. The newly created packet has all the information
      and is put into the 'pkt'.

      @note NOTUSED
  */
  static void initPacket(uint8_t* pkt, uint16_t type, uint16_t totalLength,
                         uint64_t src, uint64_t dst, uint32_t upTs,
                         uint32_t dwTs, uint8_t hops,uint16_t interface);

  /**
      @brief Returns the type of the packet
      @param uint8_t* pkt The buffer that contains the packet.
      @return Returns the type of the given packet.

      This function returns the type of the packet that is inside the
      scionCommonHeader.
  */
  static uint8_t getType(uint8_t* pkt);
  /**
      @brief Returns the Header Length of the packet
      @param uint8_t* pkt The buffer that contains the packet.
      @return Returns the Header length of the given packet.

      This function returns the header length of the packet that is inside the
      scionCommonHeader.
  */
  static uint8_t getHdrLen(uint8_t* pkt);
  /**
      @brief Returns the total length of the packet
      @param uint8_t* pkt The buffer that contains the packet.
      @return Returns the total length of the given packet.

      This function returns the total length of the packet that is inside the
      scionCommonHeader.
  */
  static uint16_t getTotalLen(uint8_t* pkt);
  /**
      @brief Returns the TimestampPtr of the packet
      @param uint8_t* pkt The buffer that contains the packet.
      @return Returns the timestamp pointer of the given packet.

      This function returns the timestamp pointer of the packet that is inside the
      scionCommonHeader. The timestamp pointer is the offset where the
      timestamp lies inside the packet. This offset can be changed depeding
      on the lengths of the source and destination address, and this is why
      this field is necessary inside the packet.

      The initial position of the packet + this offset is where the current
      timestamp is located.

      @note a single packet can contain multiple timestamps.
  */
  static uint8_t getTimestampPtr(uint8_t* pkt);
  /**
      @brief Returns the timestampInfo of the packet
      @param uint8_t* pkt The buffer that contains the packet.
      @param uint8_t* pTS The offset where the timestamp is located inside
      the packet.
      @return uint8_t The info field of the specialOpaque field that
      contains the timestamp.

      This function returns the 'info' of the timestamp. The timestamp is
      inside a special opaque field that contains the timestamp and other
      information. The info field is one of the information inside the
      special opaque field.

      The pTS field is where the special opaque field is located inside the
      packet. This is the value that is returned by the getTimestampPter()
      function.
  */
  static uint8_t getTimestampInfo(uint8_t* pkt, uint8_t pTS);
  /**
      @brief Returns the timestamp of the packet
      @param uint8_t* pkt The buffer that contains the packet.
      @return Returns the timestamp of this packet.

      This function returns the timestamp of the packet. Specifically it
      returns the timestamp that the timestamp pointer is pointing to. This
      is because there could be multiple timestamp inside a packet and the
      timestamp pointer points to the timestamp that fits the current
      situation.

      This function gets the timestamp offset and retrieves the timestamp
      value from the special opaque field that is located at that offset.
  */
  static uint32_t getTimestamp(uint8_t* pkt);
  /**
      @brief Returns the length of the Src Address of the packet.
      @param uint8_t* pkt The buffer that contains the packet.
      @return Returns the length of the source address

      This function returns the length of the source address which is inside
      the scionCommonHeader structure.
  */
  static uint8_t getSrcLen(uint8_t* pkt);
  /**
      @brief Returns the length of the Destination Address
      @param uint8_t* pkt The buffer that contains the packet.
      @return Returns the length of the destination address.

      This function returns the length of the destination address which is
      inside the scionCommonHeader structure.
  */
  static uint8_t getDstLen(uint8_t* pkt);
  /**
      @brief Returns the flag field of the packet
      @param uint8_t* pkt The buffer that contains the packet.
      @return Returns the flag field of the header.

      This function returns the flag field(8bit) which is inside the
      scionCommonHeader. Please refer to the scion implementation documents
      about this flag field.

      Flag fields are used in many ways and some of the fields are not
      currently used. The unsused flag field can be used in any case but the
      reserved flags should not be modified.
  */
  static uint8_t getFlags(uint8_t* pkt);
  /**
      @brief Returns the Current Opaque Field
      @param uint8_t* pkt The buffer that contains the packet.
      @return Returns the pointer that points to the current opaque field.

      This function returns the pointer that points to the current opaque
      field. This offset is from the beginning of the packet to the point
      where the current opaque field begins in the packet.

      pkt+offset will give the address to the current opaque field.
  */
  static uint8_t* getCurrOF(uint8_t* pkt);
  /**
      @brief Returns the Opaque Field
      @param uint8_t* pkt The buffer that contains the packet.
      @param  int offset The index of the opaque field to be returned.
      @return Returns the pointer

      This function returns a pointer to the opaque field that 'offset'
      indicates to. The offset value is actually an index value of the
      opaque field. The OPAQUE_FIELD_SIZE*offset will provide the actual
      offset from the beginning of the packet to the targeted opaque field.
  */
  static uint8_t* getOF(uint8_t* pkt, int offset);
  /**
      @brief Returns the type of the current Opaque Field
      @param uint8_t* pkt The buffer that contains the packet.
      @return Returns the type of the current opaque field.

      This function returns the type field of the current opaque field. Each
      opaque field has a type field that specifies the functionality and the
      roles of each OF. In general, this field is used as an indication if
      the current opaque field is retular or special. Regular opaque fields
      is the opaque fields inside the path and the special means any other
      type of opaque fields that are used in a special way.

      @note Please refer to the scion implementation slides for more details
      about the types of opaque fields.

  */
  static uint8_t getOFType(uint8_t* pkt);
  /**
      @brief Returns the number of opaque field.
      @param uint8_t* pkt The buffer that contains the packet.
      @return Returns the number of opaque fields inside the packet.

      This function returns the number of opaque field inside the given
      packet. More precisely, this function returns the 'numOF' value inside
      the scionCommonHeader. This value represents the number of opaque
      fields inside the packet excluding the special opaque fields. This
      value can be seen as the number of 'hops' in a very limited case.
  */
  static uint8_t getNumOF(uint8_t* pkt);
  /**
      @brief Returns the Layer 4 protocol type
      @param uint8_t* pkt The buffer that contains the packet.
      @return Returns the layer 4 protocol value of the packet.

      This function returns the l4Proto field inside the scionCommonHeader.
      @note More details will be added when this field is actually used.
  */
  static uint8_t getL4Proto(uint8_t* pkt);
  /**
      @brief Returns the new return capability field
      @param uint8_t* pkt The buffer that contains the packet.
      @return Returns the number of return capability.
  */
  static uint8_t getNRetCap(uint8_t* pkt);
  /**
      @brief Returns the capability information
      @param uint8_t* pkt The buffer that contains the packet.
      @return Returns the capability info field.
  */
  static uint8_t getCapReqInfo(uint8_t* pkt);
  /**
      @brief Returns the New capability of the packet
      @param uint8_t* pkt The buffer that contains the packet.
      @return Returns the new capability field.
  */
  static uint8_t getNewCap(uint8_t* pkt);
  /**
      @brief Returns the path validation field
      @param uint8_t* pkt
      @return uint8_t pathVal
  */
  static uint8_t getPathVal(uint8_t* pkt);
  /**
      @brief returns the srouce Authentication field
      @param uint8_t* pkt
      @return uint8_t* srcAuth
  */
  static uint8_t getSrcAuth(uint8_t* pkt);
  /**
      @brief Returns the Source Address of the packet
      @param uint8_t* pkt The buffer that
      @param uint8_t** srcAddr
      @return void

      This function gets the Src Address of the packet and puts it into the
      srcAddr.
  */
  static void getSrcAddr(uint8_t* pkt, uint8_t** srcAddr);
  /**
      @brief Returns the SrcAddr of the packet
      @param uint8_t* pkt The buffer that contains the packet.
      @return  Returns the source address as a HostAddr structure.

      This function returns a HostAddr structure that contains the source
      address. The fields inside the returned HostAddr is set depending on
      the values inside the packet. The function sees the length of the
      source address and determines the type of address. Then it sets the
      type, length and address for the HostAddr.
  */
  static HostAddr getSrcAddr(uint8_t* pkt);

  /**
      @brief Returns the Destination Address of the packet
      @param uint8_t* pkt
      @param uint8_t** dstAddr
      @return void

      This function gets the destination of the packet and puts it into the
      'dstAddr' instead of returning the value.
  */
  static void getDstAddr(uint8_t* pkt, uint8_t** dstAddr);

  /**
      @brief Returns the DstAddr of the packet
      @param uint8_t* pkt The buffer that contains the packet.
      @return  Returns the destination address as a HostAddr structure.

      This function returns a HostAddr structure that contains the
      destination address. The fields inside the returned HostAddr is set depending on
      the values inside the packet. The function sees the length of the
      destination address and determines the type of address. Then it sets the
      type, length and address for the HostAddr.
  */
  static HostAddr getDstAddr(uint8_t* pkt);
  /**
      @brief Sets the type of the packet.
      @param uint8_t* pkt The buffer that contains the packet.
      @param uint8_t type The type value.

      This function sets the type field of the packet as the given type
      value.
  */
  static void setType(uint8_t* pkt, uint8_t type);
  /**
      @brief Sets the Header Length of the packet
      @param uint8_t* pkt The buffer that contains the packet.
      @param uint8_t hdrLen The header length.

      This function sets the header length field as the given header length
      value. The type variable can be BEACON, DATA or etc. Please refer to
      the enum PacketType for all possible packet type that SCION supports.
  */
  static void setHdrLen(uint8_t* pkt, uint8_t hdrLen);
  //SLT:
  /**
      @brief Adjusts the length of the packet
      @param uint8_t* pkt The buffer that contains the packet.
      @param offset The amount of space that will be added to the packet
      length fields.

      This function adjusts the totalLength field and the header length
      field of the packet. The value 'offset' will be added to each
      totalLength and HeaderLength inside the common header.

      @note This function does NOT increase the actual size of the packet
      but ONLY adjusts the length fields in the packet.
  */
  static void adjustPacketLen(uint8_t* pkt, int offset);
  /**
      @brief Sets the total length of the packet
      @param uint8_t* pkt The buffer that contains the packet.
      @param uint16_t totalLength The total length of the packet

      This function sets the totalLength field of the given packet as the
      totalLen parameter value.
  */
  static void setTotalLen(uint8_t* pkt, uint16_t totalLen);
  /**
      @brief Sets the timestamp Ptr of the packet
      @param uint8_t* pkt The buffer that contains the packet.
      @param uint8_t tsPtr The offset to the timestamp in the packet.

      This function sets the timestampPtr field inside the common header.
      The parameter 'tsPtr' is the offset from the beginning of the packet
      to the special opaque field where the timestamp is located.
  */
  static void setTimestampPtr(uint8_t* pkt, uint8_t tsPtr);
  /**
      @brief Sets the values of the timestamp opaque filed
      @param uint8_t pkt The buffer that contains the packet.
      @param specialOpaqueField &ts

      This function sets the timestamp opaque field located at the
      'timestampPtr' inside the packet. The 'setTimestampPtr' should be
      called before this function.
  */
  static void setTimestampOF(uint8_t* pkt, specialOpaqueField& ts);
  /**
      @brief Sets the SrcLen field of the packet
      @param uint8_t* pkt
      @param uint8_t srcLen
      @return void

      Sets the srcLen field in the SCION Common header.
  */
  static void setSrcLen(uint8_t* pkt, uint8_t srcLen);
  /**
      @brief Sets the DstLen of the packet
      @param uint8_t* pkt
      @param uint8_t dstLen
      @return void

      Sets the length of the destination address in the SCION Common Header.
  */
  static void setDstLen(uint8_t* pkt, uint8_t dstLen);
  /**
      @brief Sets the flag field of the packet
      @param uint8_t* pkt
      @param uint8_t totalLen
      @return void

  */
  static void setFlags(uint8_t* pkt, uint8_t totalLen);
  /**
      @brief Sets the current opaque field pointer
      @param uint8_t* pkt
      @param uint8_t currOFPtr
      @return void

      Sets the current opaque field pointer to the given value 'currOFPtr'.
  */
  static void setCurrOFPtr(uint8_t* pkt, uint8_t currOFPtr);
  /**
      @brief Sets the total number of opaque fields in the packet.
      @param uint8_t* pkt
      @param uint8_t numOF
      @return void

      Sets the number of opaque field value in the scionCommonHeader.
  */
  static void setNumOF(uint8_t* pkt, uint8_t numOF);
  /**
      @brief Sets the layer 4 protocol of the packet
      @param uint8_t* pkt
      @param uint8_t l4Proto
      @return void

      Sets the Layer 4 protocol type value in the scionCommonHeader.
  */
  static void setL4Proto(uint8_t* pkt, uint8_t l4Proto);
  /**
      @brief Sets the NRetCap offset of the packet
      @param uint8_t* pkt
      @param uint8_t nRetCap
      @return void

      Sets the offset of the New Return Capability information in the
      header.
  */
  static void setNRetCap(uint8_t* pkt, uint8_t nRetCap);
  /**
      @brief Sets the capability request information field offset.
      @param uint8_t* pkt
      @param uint8_t capReqInfo
      @return void

      Sets the offset of capability information field.
  */
  static void setCapReqInfo(uint8_t* pkt, uint8_t capReqInfo);
  /**
      @brief Sets the new capability filed pointer
      @param uint8_t* pkt
      @param uint8_t newCap
      @return void

      Sets the new capability offset in the header.
  */
  static void setNewCapPtr(uint8_t* pkt, uint8_t newCap);
  /**
      @brief Sets the path validation pointer in the header
      @param uint8_t* pkt
      @param uint8_t* pathVal
      @return void
  */
  static void setPathValPtr(uint8_t* pkt, uint8_t pathVal);
  /**
      @brief Sets the source authentication pointer in the header.
      @param uint8_t* pkt
      @param uint8_t srcAuth
      @return void
  */
  static void setSrcAuthPtr(uint8_t* pkt, uint8_t srcAuth);
  /**
      @brief Adds length to the total length of the packet
      @param uint8_t* pkt
      @param uint16_t len
      @return void

      Adds length to the total length of the packet. This function is called
      whenever the contents inside packet is increased.
  */
  static void addTotalLen(uint8_t* pkt, uint16_t len);

  /**
      @brief Puts special opaque field in the header.
      @param uint8_t* pkt
      @param uint8_t info
      @param uint32_t timestamp
      @param uint16_t tdid
      @param uint8_t numHop
      @param uint8_t offset
      @return void

      This function sets the 'sepcial opaque field in the header with the
      information given as the parameters. The special opaque field is used
      as timestamp and other information required for forwarding packets.
      This is called 'special' because the information inside this opaque
      field is different from normal opaque field that contains forwarding
      information such as MAC.
  */
  static void putSpecialOpaque(uint8_t* pkt, uint8_t info, uint32_t
  timestamp, uint16_t tdid, uint8_t numHop, uint8_t offset);

  /**
      @brief Adds Source Address to the packet
      @param uint8_t* pkt
      @param HostAddr srcAddr
      @return void

      This function adds additional space to the packet for the source
      address and puts source information in the newly created area.
  */
  static void addSrcAddr(uint8_t* pkt, HostAddr srcAddr);
  /**
      @brief Adds Destination Address to the packet
      @param uint8_t* pkt
      @param HostAddr dstAddr
      @return void

      This function adds additional space to the packet for the destination
      address and puts destination address in the newly created area.
  */
  static void addDstAddr(uint8_t* pkt, HostAddr dstAddr);

  /**
      @brief Delets the SrcAddr from the packet
      @param uint8_t* pkt
      @return void

      This function deletes the source address from the packet
  */
  static void clearSrcAddr(uint8_t* pkt);
  /**
      @brief Deletes the DstAddr from the packet
      @param uint8_t* pkt
      @return void

      This function deletes the destination address from the packet
  */
  static void clearDstAddr(uint8_t* pkt);

  /**
      @brief Removes SrcAddr from the packet
      @param uint8_t* pkt
      @return void

      This function removes the source address from the packet along with
      the empty space in the packet.
  */
  static void removeSrcAddr(uint8_t* pkt);
  /**
      @brief Removes DstAddr from the packet
      @param uint8_t* pkt
      @return void

      This function removes the source address from the packet along with
      the empty space in the packet.
  */
  static void removeDstAddr(uint8_t* pkt);

  /**
      @brief Sets the SrcAddr of the packet
      @param uint8_t* pkt
      @param HostAddr srcAddr
      @return void

      This function sets the source address of the packet. By observing the
      values in the srcAddr, it sets the srcLen field in the
      scionCommonHeader and puts the source address into the packet.
  */
  static void setSrcAddr(uint8_t* pkt, HostAddr srcAddr);
  /**
      @brief Sets the DstAddr of the packet
      @param uint8_t* pkt
      @parma HostAddr dstAddr
      @return void

      This function sets the destination address of the packet. By observing the
      values in the srcAddr, it sets the dstLen field in the
      scionCommonHeader and puts the destination address into the packet.
  */
  static void setDstAddr(uint8_t* pkt, HostAddr dstAddr);

  /**
      @brief Sets the path of the packet
      @param uint8_t* pkt
      @param uint8_t* path
      @param uint8_t pathLen
      @param uint8_t numOF
      @return void

      This function adds the path information to the packet and sets the
      number of opaque field and other header values that matches the new
      path.
  */
  static void setPath(uint8_t* pkt, uint8_t* path,
  uint8_t pathLen,uint8_t numOF);

  /**
      @brief Adds numOF to the numOF field in the header
      @param uint8_t* pkt
      @param uint8_t numOF
      @return void

      Increase the number of opaque field with the given numOF field. It
      simply adds the numOF value to the numOF field in the
      scionCommonHeader.
  */
  static void addNumOF(uint8_t* pkt, uint8_t numOF);

  //SL: added for the new packet format
  /**
      @brief Returns the current opaque field pointer of the packet
      @param uint8_t* pkt
      @return uint8_t
  */
  static uint8_t getCurrOFPtr(uint8_t * pkt);
  /**
      @brief Returns current timestamp pointer
      @param uint8_t* pkt
      @return Returns the time stamp pointer as uint8_t.

      This function returns the current time stamp pointe which is a offset
      value from the start of the packet to the timestamp field.
  */
  static uint8_t getCurrentTimestampPtr(uint8_t * pkt);
  /**
      @brief Returns timestamp value
      @param uint8_t* pkt The packet that will be examined.
      @param ts_ptr The timestamp ptr (offset).

      This function leverages the return value of getCurrentTimestampPtr
      which is the offset from the start of the packet to the timestamp
      field. By addeing pkt+ts_ptr it returns the value inside that address.
  */
  static uint32_t getTimestamp(uint8_t * pkt, uint8_t ts_ptr);
  /**
      @brief Returns true if the given opaque field is regular.
      @param uint8_t* pkt The packet to be examined.
      @param uint8_t the current opaque field pointer.

      This function examines the opaque field at the 'pCurrOF' and checks if
      it is a regular opaque field. Returns 1 if the opaque field is
      regular, and 0 otherwise.
  */
  static uint8_t isRegularOF(uint8_t * pkt, uint8_t pCurrOF);

  static uint8_t getOFPtr(uint8_t* pkt);
  //increase OF pointer by cnt OFs, and return the current OF pointer
  static uint8_t increaseOFPtr(uint8_t * pkt, uint8_t cnt);
  static void setUppathFlag(uint8_t* pkt);
  static void setDownpathFlag(uint8_t* pkt);
  static uint16_t getEgressInterface(uint8_t* pkt);   //returns the egress interface id in a PCB
  static uint16_t getIngressInterface(uint8_t* pkt);  //returns the ingress interface id in a PCB
  static uint16_t getOutgoingInterface(uint8_t* pkt); //returns the outgoing interface id
                            //in the current forwarding path
                            //(differ whether a packet is on the uppath or the downpath)
  //set scionHeader. This simplifies packet construction
  static uint8_t setHeader(uint8_t * packet, scionHeader & hdr,
                           uint8_t type=WITH_OF);
  //SL: added to expedite packet header handling
  static uint8_t getCommonHeader(uint8_t * packet, scionCommonHeader & hdr);
  static uint8_t * getData(uint8_t * packet);   //returns the pointer to the payload
  static uint8_t * getFirstOF(uint8_t * packet);  //returns the pointer to the first opaque field

  private:
};

#pragma pack(pop)

#endif

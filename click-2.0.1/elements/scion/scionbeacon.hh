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

#ifndef SCIONBEACON_HH_INCLUDED
#define SCIONBEACON_HH_INCLUDED

#include <stdint.h>
#include <memory.h>
#include "scioncryptolib.hh"
#include "packetheader.hh"
#include "define.hh"


/**
    @brief Structure Representation of the Path Construction Beacon
    This structure is used when the RAW PCB packet is paresed in the beacon
    server. 
*/
struct pcb{
    /** Total Length of the PCB (packet length)*/
    uint16_t totalLength;
    /** Hops included in the PCB */
    uint8_t hops;
    /** Timestamp of this PCB */
    uint32_t timestamp;
    /** Raw PCB in bytes */
    uint8_t* msg; //original markings in PCB
    /** Propagated Flag. 1 if it is already propagated */
    uint8_t propagated;
    /** Registered Flag. 1 if it is already Registered */
    uint8_t registered;
    /** Ingress interface ID (the interface this PCB came in) */
    uint16_t ingress; /*the interface id that this pcb used when entering this ad*/
    /** Age of this PCB */
    uint32_t age;
};

/**
    @brief Opaque Field Structure Representation
    This struct is used to store the opaque field generation key.
*/
struct ofgKey{
    /** Time value */
	time_t time;
    /** The actual key in bytes */
    uint8_t key[OFG_KEY_SIZE];
	/** AES context that will be used in polarssl library */
	aes_context actx;
};

//SL: why functions are defined as a friend?
//it seems unnecessary and needs to be revised.
struct scionHash{
    unsigned char hashVal[SHA1_SIZE];

    scionHash() {
        memset(hashVal, 0, SHA1_SIZE);
    }

    void clear() {
        memset(hashVal, 0, SHA1_SIZE);
    }

    friend bool operator<(scionHash const& h1, scionHash const& h2){
        for(int i=0;i<SHA1_SIZE;i++){
            if(h1.hashVal[i]==h2.hashVal[i]){
                continue;
            }else{
                return h1.hashVal[i]<h2.hashVal[i];
            }
        }
        return false;
    }

    friend bool operator==(scionHash const& h1, scionHash const& h2){
        for(int i=0;i<SHA1_SIZE;i++){
            if(h1.hashVal[i]!=h2.hashVal[i]){
                return false;
            }
        }
        return true;
    }


    friend bool operator!=(scionHash const& h1, scionHash const& h2){
        for (int i = 0; i < SHA1_SIZE; i++) {
            if (h1.hashVal[i] != h2.hashVal[i])
                return true;
        }
        return false;
    }

    scionHash& operator=(const scionHash& val) {
        for (int i = 0; i < SHA1_SIZE; i++) {
            hashVal[i] = val.hashVal[i];
        }
        return *this;
    }

    scionHash& operator=(const int& val) {
        memset(hashVal, val, SHA1_SIZE);
        return *this;
    }



    void print() {
        for (int i = 0; i < SHA1_SIZE; i++) {
            printf("%x", hashVal[i]);
        }
        printf("\n");
    }
};

/**
    @brief SCION Beacon Library
    
    This is a library class that contains functions to manipulate PCB packets. All
    the 'pkt' values must be a PCB packet, and if not, the behavior is unspecified
    and most of the case the functions will return an error. 

    In order to understand and use the funtion correctly, the user must understand
    the concept of a link in SCION beacons and how they are represented as
    structures.

    SCION links. 
    
    SCION links are a set of information that an AD puts into the PCB packets.
    The information includes ingress interface, egress interface, opaque fields
    and any other necessary values. Please refer to the scion implementation
    document and the packetheader.hh for more information about the links. 
    The links are used at the end point ADs when creating end-to-end paths. The
    information reveals the path information of the received PCBs and the
    end-point ADs choose up/down paths by examining the information inside the
    links. 
    
    Then only the opaque field inside the links are exctracted, piled into an
    array to form a path, and put into the DATA packet to forward packets.
    
    pcbMarkings/peerMarking structues. 
    
    The SCION links are represented as pcbMarking structure and peerMakring
    structures. These structures are defined inside the packetheader.hh. The
    fields inside these structures exactly follows the format of scion links. 
    
    One thing to point out at this point is the difference between the pcbMarkings
    and peerMarkings. The pcbMarking structure is used to store information for
    the 'up-down' link. In SCION, each AD specifies one 'up-down' link that
    has a pair of routers that the packet will travel when going through this AD. 
    This pair is only for those packets that travers from downstream to upstream
    or from upstream to downstream. Any peering relationships are not defined with
    this structure. 
    
    Instead of using pcbMarking for peering relationship, the peerMarking
    strucuture is used. Similar to pcbMarking, peerMarking structure contains the
    fields that are necessary for sending packets through the peering routers. 
    
    The main difference between the two is the number of markings allowed in a
    single PCB. SCION allows an AD to have upto only one up-down pcbMarkings while
    allowing multiple peerMarkings. In other words, an AD can have only one
    up-down links and have multiple peering links.    

    specialOpaqueField for PCBs

    The PCB packets have special opaque fields that are only used in PCB
    operations. Specifically PCBs have two special opaque fields. The one with the
    timestamp, the same special opaque field in the forwarding packet, and the
    other special opaque field with the number of hops and interface values. The
    latter is used when sending and manipulating PCBs for the servers and routers
    and alos the end hosts. This field will be only used at the PCB processing
    step, and removed afterwards.
    
    Some of the functions in this class are for the fields inside this special
    opaque field, and the names of these are similar to those in the
    packetheader.hh. Please don't be confused.  
*/
class SCIONBeaconLib{
public:
    /**
        @brief Helper function that prints the PCB information to the stdout. 
    */
    static void printPCB(uint8_t* pkt);

    /**
        @brief Creates MAC for the opaque field
        @param uint32_ts The timestamp value. 
        @param uint8_t exp The expire time value. 
        @param uint16_t ingress Ingress interface ID. 
        @param uint16_t egress Egress interface ID. 
        @param uint64_t prev_of Previous opaque field. 
        @param aes_context* ctx The Opaque Field generating key. 
        @return Returns the 32 bit MAC value. 

        This function generates a new MAC value using the parameters and
        encrypting with the given AES key. This function is a wrapper function
        that builds a byte array that contains all the parameter except for the
        key, and pass the array to the genMAC() function. 
        
        genMAC will return the newly generated MAC value and this function return
        whatever genMAC() returns. 
        
        The main functionality of this function is to build a byte array that
        contains all the necessary fields in the correct order.  
    */
	static uint32_t createMAC(uint32_t ts, uint8_t exp, uint16_t ingress, uint16_t egress, uint64_t prev_of,
            aes_context* ctx);

    /**
        @brief Verifies the validity of the MAC
        @param uint32_t ts The timestamp value. 
        @param uint8_t exp The expieration time value. 
        @param uint16_t ingress Ingress interface ID.
        @param uint16_t egress Egress interface ID. 
        @param uint64_t prev_of Previous opaque field. 
        @param uint32_t mac The MAC that will be checked. 
        @param aes_context* ctx The Opaque field generating key. 

        @return Returns SCION_SUCCESS on success, SCION_FAILURE on failure.  

        This function checks the validy of the MAC value that is passed by the
        parameter. The parameter 'uint32_t mac' will be checked. This function
        will build an entirely new MAC value using the given parameters and check
        to see if the new MAC value and the old MAC (parameter) are same. 

        If the two values are equal, the given MAC is considered as valid and this
        function returns SCION_SUCCESS. If not the function will return
        SCION_FAILURE. 
    */
	static int verifyMAC(uint32_t ts, uint8_t exp, uint16_t ingress, uint16_t egress, uint64_t prev_of, 
    uint32_t mac,aes_context* ctx);

    static uint8_t verifyPCB(uint8_t* pkt);

    /**
        @brief Adds link to the PCB
        @param uint8_t* pkt The buffer that contains the packet. 
        @param uint16_t ingress The ingress interface. 
        @param uint16_t egress The egress interface. 
        @param uint8_t type The packet type value. 
        @param uint64_t aid The AD AID value. 
        @param uint32_t tdid The TD AID value. 
        @param aes_context* ctx The opaque field generating key.
        @param uint8_t ofType The opaque field type value. 
        @param uint8_t exp The expiration time value. 
        @param uint16_t bwAlloc Bandwidth value 
        @param uint16_t sigLen The length of the included signature. 
        @return Returns SCION_SUCCESS on success, SCION_FAILURE in any error.  

        This function adds a 'link' to the packet. The 'link' here is represented
        by the pcbMarking structure defined in the packetheader.hh. This contains
        all the necessary information about the AD that is putting information to
        this PCB, including the ingress and egress interfaces and the opaque
        field. 
        
        Each AD creates and puts a pcbMarking into the PCB indicating that any
        downstream AD that gets this information is allowed to send the packet
        through itself. Also, the AD provides the information that it requires in
        order to check if the access is actually granted.
        
        In other words, the link is the information is used by the end point ADs
        to forward packets. The opaque fields in the links are extracted and piled
        in to a list, and this list is the path.
        
        There are two types of links that a single AD can add. One is a 'up-down'
        link which specifies a pair of ingress and egress routers that a packet
        will pass during the 'packet Transit'. The path of the packet will be
        strictly from a client to the provider or provider to customer. 
        
        The other type of link is peering links. This link is defined as
        peerMarking. Similar to the pcbMarking, the information needed to send a
        packet to a peering link is stored inside the peerMarking structure. 
        
        In conclusion, each AD must specify up to one 'up-down' link and many
        peerin links as much as they want. If an AD wants to specify a different
        pair of routers for the up-down links, it must put that marking in a
        different PCB.
        
        Please refer to the packetheader.hh for detail information about the link
        structures.        
    */
	static uint8_t addLink(uint8_t* pkt,uint16_t ingress,uint16_t egress,
    uint8_t type, uint64_t aid, uint32_t tdid, aes_context* ctx, uint8_t ofType,
    uint8_t exp,uint16_t bwAlloc, uint16_t sigLen);

    /**
        @brief Adds a peering link to the PCB packet  
        @param uint8_t* pkt The buffer that contains the packet. 
        @param uint16_t ingress The ingress interface. 
        @param uint16_t egress The egress interface
        @param uint16_t pegress The peer-egress interface. 
        @param uint8_t type The packet type value. 
        @param uint64_t aid The AD AID value. 
        @param uint32_t tdid The TD AID value. 
        @param aes_context* ctx The opaque field generating key. 
        @param uint8_t ofType Opaque field type of the OF inside the link. 
        @param uint8_t exp The expiration time value. 
        @param uint16_t bwAlloc The bandwidth allocation field in common Header. 
        @return Returns SCION_SUCCESS on success, SCION_FAILURE on any error. 

        This function adds a link, but in this case it is the peering information.
        The 'link' in the addLink() function means the transit relation between up
        and down ADs but the 'link' in this function means the connection between
        two peering ADs. The inforamtion is very similar in both cases but some of
        the modification is necessary in the peering link. 
        
        @note The detailed description of peering links is in the class details.  
    */
	static uint8_t addPeer(uint8_t* pkt, uint16_t ingress, uint16_t egress, 
    uint16_t pegress, uint8_t type, uint64_t aid, uint32_t tdid, 
    aes_context* ctx,uint8_t ofType, uint8_t exp, uint16_t bwAlloc);
    
    /**
        @brief Signs the packet
        @param uint8_t* pkt The buffer that contains the packet. 
        @param int sigLen The length of the signature that will be added to the
        packet. 
        @param uint64_t &next_aid The AID of the next AD. 
        @param rsa_context* ctx The RSA private key for signature generation. 
        @return Returns SCION_SUCCESS on success.  

        This function signs the given packet with the given RSA private key. The
        length of the signature is specified as the sigLen parameter. This
        function only should be called after all the links and peer links are
        added to the PCB. 
        
        This function is not for signing all the SCION packets but only for
        signing the PCB only after all the necessary information is added to the
        PCB.
        
        Failure of adding proper information before calling this function may
        cause unknown behavior or crash.   
    */
    static uint8_t signPacket(uint8_t* pkt, int sigLen, uint64_t & next_aid, 
    rsa_context* ctx);

    /**
        @brief Returns the pcb marking of the packet.
        @param uint8_t* packet The buffer that contains the packet. 
        @param uint16_t offset The offset from the beginning of the packet to the
        location of the pcb marking. 
        @return Returns the relative position to the pcbMarking inside the packet. 

        This function returns a pointer that points to the pcb marking that is
        located at the 'packet start + offset'. Since the offset is a relative
        value, this function returns the absoulte pointer value for the
        pcbMarking. 
    */
    static pcbMarking* getPcbMark(uint8_t* packet, uint16_t offset);

    /**
        @brief Returns the next pcb marking of the packet
        @param pcbMarking* mrkPtr The pcbMarking of the current marking. 
        @return The pointer that points to the next (if any) pcbMarking 

        This function returns the next pcbMarking. Here, the 'next' marking means
        the marking of the AD that is located after this current marking. 

        This function is used as an iterating function for the pcb markings inside
        the PCB.
        
        @note This function must called on PCBs that HAVE signatures. If the PCB
        does not contain signature the getNextPcbMark() should be called.  
    */
    static pcbMarking* getNextPcbMark(pcbMarking* mrkPtr);

    /**
        @brief Returns the next pcb marking without the signature
        @param pcbMarking* mrkPtr The current marking pointer. 
        @return Returns a pointer that points to the next marking. .

        This function works in a similar way as the getNextPcbMark() function but
        this fuction is for the PCB that the signatures are removed. In some step
        inside the Beacon server, the signatures are removed from the packet to
        reduce the size of the packet. 
        
        This function is used as an iterative function in those situations.
        
        @note This function must be called on pcb packets that the signatures are
        removed. If the signatures are NOT removed the getNextPcbMark() should be
        called.   
    */
    static pcbMarking* getNextPcbMarkNoSig(pcbMarking* mrkPtr);

    /**
        @brief Returns the peer marking of the packet
        @param pcbMarking* mrkPtr The pcb marking that points to current marking. 
        @return Returns a pointer that points to the first(if any) peering links
        inside the pcb. If there are no peering links found, the function returns
        NULL. 

        This function returns a pointer to the first peering marking among the
        peering links that an AD included. 
        
        This function returns the first peering that the current AD, specified
        inside the pcbMarking, added. This is not the first peer marking inside
        the packet. 
    */
    static peerMarking* getPeerMarking(pcbMarking* mrkPtr);

    /**
        @brief Returns next peer marking of the packet. 
        @param peerMarking* peerPtr The pointer that points to a peering link. 
        @return Returns the next peering link pointer. If there isn't any, the
        function returns NULL.  

        This function returns the next peering marking pointer if there is any.
        The function uses the size of the peerMarking structure to get tne next
        pointer address. If the new address of the pointer does not point to a
        peerLink, the function returns NULL.  
    
    */
    static peerMarking* getNextPeer(peerMarking* peerPtr);

    /**
        @brief Initializes PCB information for the new PCB. 
        @param uint8_t* pkt The buffer that contains the packet. 
        @param uint32_t timestamp Timestamp value. 
        @param uint16_t tdid TD AID value. 
        @param uint32_t ROTVer ROT version of the current packet. 
    
        This function initializes the PCB packet. This function is usually called
        from the TD core Beacon Server when creating a new PCB to send down. This
        function allows the Beacon Servers to initialize some values of the newly
        created PCBs. 
        
        @note The uint8_t* pkt MUST have enough space allocated (static or
        dynamic) before calling this function.  
    */
    static void initBeaconInfo(uint8_t* pkt, uint32_t timestamp, uint16_t tdid, uint32_t ROTver);

    /**
        @brief Sets the number of hops in the special opaque field
        @param uint8_t* pkt The buffer that contains the packet. 
        @param uint8_t hops The number of hops in this BEACON. 
        @note Beacon Special Opaque Field function. 
    
        Sets the nubmer of hops of inside the special opaque field.
    */
    static void setNumHops(uint8_t* pkt, uint8_t hops);
    
    /**
        @brief Increases the number of hops in the special opaque field
        @param uint8_t* pkt The buffer that contains the packet. 
        @param uint8_t hops The number of hops to be added. 
        @note Beacon Special Opaque Field function. 

        Increases the number of hops in the special opaque field.
    */
    static void addNumHops(uint8_t* pkt, uint8_t hops);

    /**
        @brief Returns the number of hops 
        @param uint8_t* pkt The buffer that contains the packet. 
        @return Returns the number of hops inside the special opaque field.  
        @note Beacon Special Opaque Field function. 

        Returns the number of hop value inside the special opaque field, NOT from
        the header. 
    */
    static uint8_t getNumHops(uint8_t* pkt);

    /**
        @brief Returns the ROT version of the packet
        @param uint8_t* pkt The buffer that contains the packet. 
        @return Returns the ROT version number from the special opaque field. 
        @note Beacon Special Opaque Field function. 

        Returns the ROT version number from the special opaque field. 
    */
    static uint32_t getROTver(uint8_t* pkt);

    /**
        @brief Sets the ROT version of the packet
        @param uint8_t* pkt The buffer that contains the packet. 
        @param uint32_t ROTVer The new ROT version number. 
        @note Beacon Special Opaque Field function. 

        This function sets the ROT version field inside the special opaque field
        to a new value. 
        
    */
    static void setROTver(uint8_t* pkt, uint32_t ROTver);

    /**
        @brief Sets the interface id of the packet
        @param uint8_t* pkt The buffer that contains the packet. 
        @param uint16_t interface The interface id value. 
        @note Beacon Special Opaque Field function. 

        This function sets the interface field in the special opaque field to a
        new value. 
    */
    static void setInterface(uint8_t* pkt, uint16_t interface);

    /**
        @brief Returns the interface id of the packet
        @param uint8_t* pkt The buffer that contains the packet. 
        @return Returns the interface field inside the special opaque field. 
        @note Beacon Special Opaque Field function. 

        This function returns the interface id from the special opaque field. This
        function has nothing to do with the interface ids in the opque field. 
    */
    static uint16_t getInterface(uint8_t* pkt);

};

//CLICK_ENDDECLS

#endif 

/*
 * SCION Beacon Functions
 * 
 * Sangjae Yoo <sangjaey@gmail.com>
 * 
 * 2011-10-10
 */

#ifndef SCIONBEACON_HH_INCLUDED
#define SCIONBEACON_HH_INCLUDED

#include <stdint.h>
#include <memory.h>
#include "scioncryptolib.hh"
#include "packetheader.hh"
#include "define.hh"

struct pcb{
    uint16_t totalLength;
    uint8_t hops;
    uint32_t timestamp;
    uint8_t* msg; //original markings in PCB
    uint8_t propagated;
    uint8_t registered;
    uint16_t ingress; /*the interface id that this pcb used when entering this ad*/
    uint32_t age;
};

//SL:NK
struct ofgKey{
	time_t time;
    uint8_t key[OFG_KEY_SIZE];
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

class SCIONBeaconLib{
public:
    static void printPCB(uint8_t* pkt);

    static uint32_t createMAC(uint32_t ts, uint8_t exp, uint16_t ingress, uint16_t egress, uint64_t prev_of,
            aes_context* ctx);

    static int verifyMAC(uint32_t ts, uint8_t exp, uint16_t ingress, uint16_t egress, uint64_t prev_of, 
    uint32_t mac,aes_context* ctx);

    static uint8_t verifyPCB(uint8_t* pkt);

    static uint8_t addLink(uint8_t* pkt,uint16_t ingress,uint16_t egress,
    uint8_t type, uint64_t aid, uint32_t tdid, aes_context* ctx, uint8_t ofType,
    uint8_t exp,uint16_t bwAlloc, uint16_t sigLen);

    static uint8_t addPeer(uint8_t* pkt, uint16_t ingress, uint16_t egress, 
    uint16_t pegress, uint8_t type, uint64_t aid, uint32_t tdid, 
    aes_context* ctx,uint8_t ofType, uint8_t exp, uint16_t bwAlloc);

    static uint8_t signPacket(uint8_t* pkt, int sigLen, uint64_t & next_aid, 
    rsa_context* ctx);

    static pcbMarking* getPcbMark(uint8_t* packet, uint16_t offset);

    static pcbMarking* getNextPcbMark(pcbMarking* mrkPtr);

    static pcbMarking* getNextPcbMarkNoSig(pcbMarking* mrkPtr);

    static peerMarking* getPeerMarking(pcbMarking* mrkPtr);

    static peerMarking* getNextPeer(peerMarking* peerPtr);

    static void initBeaconInfo(uint8_t* pkt, uint32_t timestamp, uint16_t tdid, uint32_t ROTver);

    static void setNumHops(uint8_t* pkt, uint8_t hops);
    
    static void addNumHops(uint8_t* pkt, uint8_t hops);

    static uint8_t getNumHops(uint8_t* pkt);

    static uint32_t getROTver(uint8_t* pkt);

    static void setROTver(uint8_t* pkt, uint32_t ROTver);

    static void setInterface(uint8_t* pkt, uint16_t interface);

    static uint16_t getInterface(uint8_t* pkt);

};

//CLICK_ENDDECLS

#endif 

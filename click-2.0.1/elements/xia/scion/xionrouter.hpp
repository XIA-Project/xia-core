
#ifndef XIONROUTER_HEADER_HH_
#define XIONROUTER_HEADER_HH_
#include <map>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <sys/time.h>
/*include here*/
#include "packetheader.hh"
#include "scionbeacon.hh"
#include "define.hh"
#include "config.hh"

class XionRouter{ 
    public :
        XionRouter(string);
        XionRouter();
        ~XionRouter();

	private:
        int handle_packet(uint8_t* inc_pkt, int port, uint8_t**);
        bool getOfgKey(uint32_t timestamp, aes_context &actx);
        void initVariables();

        int forwardDataPacket(int port, uint8_t * pkt);
        int verifyInterface(int port, uint8_t* packet, uint8_t dir);
        int verifyOF(int port, uint8_t* pkt); //verify opaque field (not crossover point)
        bool initOfgKey();
        bool updateOfgKey();
        int normalForward(uint8_t type, int port, uint8_t * packet, uint8_t uppath);
        int crossoverForward(uint8_t type, uint8_t info, int port, uint8_t * packet, uint8_t uppath);
    private:
		//SLT
        string m_sConfigFile;
        std::map<uint32_t, aes_context*> m_OfgAesCtx;

        ofgKey m_currOfgKey;
        ofgKey m_prevOfgKey;
		
        uint8_t m_uMkey[16];
};
#endif

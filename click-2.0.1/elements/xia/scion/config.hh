#ifndef _CONFIG_H
#define _CONFIG_H
#include <stdint.h>
#include <vector>
#include "define.hh"

class Config {

public: 
    Config(){
        memset(m_csPrivateKeyFile, 0, MAX_FILE_LEN+1);
        memset(m_csCertificateFile, 0, MAX_FILE_LEN+1);
        memset(m_csMasterOfgKey, 0,  MASTER_KEY_LEN+1);
        memset(m_csLogFile, 0, MAX_FILE_LEN+1);
        memset(m_csPSLogFile, 0, MAX_FILE_LEN+1);
        memset(m_csCSLogFile, 0, MAX_FILE_LEN+1);
        memset(m_csBSLogFile, 0, MAX_FILE_LEN+1);
        memset(m_csSLogFile, 0, MAX_FILE_LEN+1);
        memset(m_csMasterADKey, 0, MASTER_KEY_LEN+1);
        
        m_iRegisteredPaths = NUM_REG;
        m_iPTime =PROP_TIME;
        m_iRTime =REG_TIME;
        m_iResetTime = RESET_TIME;
        m_iNumInterface=0;
        m_iLogLevel=LOG_LEVEL;
        m_iIsRegister=0;
        m_iCore=0;
        m_iPCBQueueSize=BEACON_QUEUE_SIZE;
        m_iPSQueueSize= PS_QUEUE_SIZE; 
        m_uAdAid=0;
        m_uTdAid=0;
        m_uAddrBS=0;
        m_uAddrPS=0;
        m_uAddrCS=0;
		m_iPCBGenPeriod=0;
    }



private:
    uint64_t m_uAdAid;
    uint64_t m_uTdAid;
	uint64_t m_uAddrBS;
	uint64_t m_uAddrPS;
	uint64_t m_uAddrCS;
	
    char m_csPrivateKeyFile[MAX_FILE_LEN+1];
	char m_csCertificateFile[MAX_FILE_LEN+1];
	char m_csMasterOfgKey[MASTER_KEY_LEN+1];
    char m_csLogFile[MAX_FILE_LEN+1];
    char m_csPSLogFile[MAX_FILE_LEN+1];
    char m_csCSLogFile[MAX_FILE_LEN+1];
    char m_csBSLogFile[MAX_FILE_LEN+1];
    char m_csSLogFile[MAX_FILE_LEN+1];
    char m_csMasterADKey[MASTER_KEY_LEN+1];
    char m_tunnelSource[16];
    char m_tunnelDestination[16];
     
    std::vector<uint16_t> ifid_set;
    int m_iNumInterface;    
    int m_iRegisteredPaths;
    int m_iShortestUPs;
    int m_iPTime;
    int m_iRTime;
    int m_iResetTime;    
    int m_iLogLevel;
    int m_iIsRegister;
    int m_iCore;
    int m_iPCBQueueSize;
    int m_iPSQueueSize;
	int m_iPCBGenPeriod;
public:
	bool parseConfigFile(char* filename);
    int getNumRegisterPath(){ return m_iRegisteredPaths; }

    int getNumRetUP(){ return m_iShortestUPs; }
    
    int getIsRegister(){ return m_iIsRegister;}
    int getIsCore(){ return m_iCore; }
    int getPCBQueueSize(){ return m_iPCBQueueSize;}
    int getPSQueueSize(){ return m_iPSQueueSize;}
    uint64_t getBeaconServerAddr() {return m_uAddrBS;}
    uint64_t getPathServerAddr() { return m_uAddrPS;}
    uint64_t getCertificateServerAddr() { return m_uAddrCS;}
    uint64_t getAdAid() { return m_uAdAid; }
    uint64_t getTdAid() { return m_uTdAid; }
   	int getPCBGenPeriod() { return m_iPCBGenPeriod; } 

    int getLogLevel(){
        return m_iLogLevel;
    }


    bool getSwitchLogFilename(char *fn){
        strncpy(fn, m_csSLogFile, MAX_FILE_LEN);
        return true;
    }

    bool getLogFilename(char *fn){
        strncpy(fn, m_csLogFile, MAX_FILE_LEN);
        return true;
    }
	
    bool getCSLogFilename(char *fn){
        strncpy(fn, m_csCSLogFile, MAX_FILE_LEN);
        return true;
    }

    bool getPSLogFilename(char *fn){
        strncpy(fn, m_csPSLogFile, MAX_FILE_LEN);
        return true;
    }

    bool getPCBLogFilename(char *fn){
        strncpy(fn, m_csBSLogFile, MAX_FILE_LEN);
        return true;
    }

    bool getPrivateKeyFilename(char * fn) {strncpy(fn, m_csPrivateKeyFile,
        MAX_FILE_LEN); return true;}
	
    bool getCertFilename(char * fn) {strncpy(fn,
        m_csCertificateFile,MAX_FILE_LEN); return true;}
    
    bool getOfgmKey(char* key){ strncpy(key, m_csMasterOfgKey, MASTER_KEY_LEN);
        return true;}
    
    bool getMasterADKey(char* key){ strncpy(key, m_csMasterADKey, MASTER_KEY_LEN);
        return true;}
    int getK(){return m_iRegisteredPaths;}
    
    int getPTime(){return m_iPTime;}
    
    int getRTime(){return m_iRTime;}
    
    int getResetTime(){return m_iResetTime;}

    std::vector<uint16_t> getIFIDs(){return ifid_set;}

    const char * getTunnelSource() {
        return m_tunnelSource;
    }

    const char * getTunnelDestination() {
        return m_tunnelDestination;
    }

};


#endif

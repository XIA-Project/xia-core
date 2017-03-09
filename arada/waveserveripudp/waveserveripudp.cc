/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * TCP server that provides a simple tx/rx interface to remote nodes.
 * Packets are sent using UDP/IP over WAVE.
 * Only supports a single client at a time.
 *
 * Rui Meireles (rui@cmu.edu)
 * Summer 2016
 */

// c++ standard headers
#include <string> // std::string
#include <fstream> // std::ifstream
#include <iostream> // std::cout, std::cerr
#include <sstream> // std::istringstream, std::ostringstream
#include <thread> // std::this_thread::sleep_for()
#include <iomanip> // std::hex, etc
#include <map> // std::map
#include <vector> // std::vector
#include <chrono> // std::chrono::system_clock
#include <thread> // std::this_thread

// legacy c standard headers
#include <csignal> // signal()
#include <cstdlib> // strtoul()
#include <cstdint> // uint*_t
#include <cstring> // strerror(), memcpy(), memset()
#include <cassert> // assert()
#include <ctime> // std::time_t

// unix headers
#include <unistd.h> // getpid()
#include <pthread.h> // pthread_*()
#include <arpa/inet.h> // htons(), ntohs(), etc
#include <sys/socket.h> // socket(), setsockopt(), bind(), ...
#include <errno.h> // errno

// my headers
#include "configfile.h"
#include "pdrlog.h"

// Global definitions
#define MTU 66000 // let's support 64K jumbo packets, just in case...

#define CLICK_CON_CODE 254 // click connect code, so we know someone's listening
#define CLICK_DISC_CODE 255 // click disconnect code, so we know it's gone

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

// make life with chrono easier
using secs = std::chrono::seconds;
using sysclock = std::chrono::system_clock;


/* Global variables, aren't they fun */
pthread_t _rcvrTid=0, _loggerTid=0;
int _clickSockFd=0, _waveSockFd=0;
struct sockaddr_in _clickCliAddr;
bool _isClickCliConn = false;
PdrLog _pdrLog;
bool _writePdrLog = false;

#ifdef DEBUG
unsigned long long nTx=0, nRx=0; // for debug only
#endif

struct RcvThdArgs {
  uint64_t myWaveMacInt;
};

struct LgrThdArgs{
  PdrLog& pdrLog;
  std::time_t timeres;
  const std::string& logFname;
  const std::string& myWaveMacStr;
};


/**
 * Convenience method to split a string
 */
int stringSplit(const std::string &str, char c,
                std::vector<std::string> &splinters){

  std::string accStr;
  int nstrings = 0;

  for(std::string::const_iterator itr=str.begin(); itr != str.end(); ++itr){

    if (*itr == c){ // save the collected string in the vector

      splinters.push_back(accStr);
      accStr.clear();
      nstrings++;

    } else{ // push the character onto the end of the string
      accStr.push_back(*itr);
    }
  }
  // at the end of it all, push the remainder, if there is some
  if (accStr.length() > 0){
    splinters.push_back(accStr);
    nstrings++;
  }

  return nstrings;
}


/**
 * Extract a MAC address
 * Returns 0 on success, -1 on failure.
 */
int getMacIntFromStr(const std::string &macStr, uint64_t &macInt){

  // transform the mac address into an integer value
  std::vector<std::string> macSplinters;
  stringSplit(macStr, ':', macSplinters);

  if (macSplinters.size() != 6){ // there must be 6 bytes in a mac address
    return -1;
  }

  uint8_t macIntArray[6];
  int i = 0;
  for (std::vector<std::string>::const_iterator itr = macSplinters.begin();
      itr != macSplinters.end(); ++itr) {
    
    macIntArray[i++] = strtoul(itr->c_str(), NULL, 16); // extract
  }
  
  // copy it over
  macInt = 0;
  std::memcpy(&macInt, &macIntArray, 6);

  return 0;
}


/**
 * Reads a file with mac->ip mapping information, line by line.
 * The format is one (mac, ip) pair each line, separated by a space.
 *
 * Returns the number of entries succsessfully read.
 */
int loadMacIpMap(const std::string &macIpMapFname,
                 std::map<uint64_t,std::string> &macIpMap){

  int npairs = 0;
  std::ifstream infile(macIpMapFname);

  std::string line;
  while (std::getline(infile, line)){

    std::istringstream iss(line);
    std::string macStr, ipStr;
    
    if (line[0] == '#'){ // skip comment lines
      continue;
    }
    

    if (not (iss >> macStr >> ipStr)){

#ifdef DEBUG
      std::cout << "macIpMap \"" << macIpMapFname << \
          "\", skipping malformed line: " << line << std::endl;
#endif

      continue;
    } // line not in the expected format

    // process (mac,ip) pair
    uint64_t macInt;
    
    if (getMacIntFromStr(macStr, macInt)){
      std::cerr << "Skipping invalid mac: " << macStr << std::endl;
      continue; // on to the next line
    }

    // add (mac,ip) pair to map
    macIpMap[macInt] = ipStr;
    npairs++;
  }

  return npairs;
}


/**
 * Kills the program the poise and grace of a harakiri.
 */
void die(){

  // cancel the receiver thread (not joined because infinite loop)
  if (_rcvrTid != 0 and pthread_cancel(_rcvrTid)){
    std::cerr << "die(), pthread_cancel(_rcvrTid): " << strerror(errno) << \
        std::endl;
  }
  
  // cancel the logger thread (not joined because infinite loop)
  if (_loggerTid != 0 and pthread_cancel(_loggerTid)){
    std::cerr << "die(), pthread_cancel(_loggerTid): " << strerror(errno) << \
        std::endl;
  }

  // close the click comm socket
  if (_clickSockFd != 0){
    if (close(_clickSockFd) == -1){
      std::cerr << "die(), close(_clickSockFd): " << strerror(errno) << \
          std::endl;
    }
  }

  // close the wave socket
  if (_waveSockFd != 0){
    if (close(_waveSockFd) == -1){
      std::cerr << "die(), close(_waveSockFd): " << strerror(errno) << \
          std::endl;
    }
  }

  exit(0); // that's all folks!
}


/**
 * Catches signals.
 */
void sigHandler(int /*sigNum*/){
  die();
}


void* receiver_thread(void *arg){

  RcvThdArgs *thdArgs = (RcvThdArgs *) arg;
  uint64_t myWaveMacInt = thdArgs->myWaveMacInt;

  // this is an infinite-loop type thread
  // join() won't work so enable cancellation
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  assert(_waveSockFd != 0); // the socket must be ready by thread launch!

  uint8_t rcvBuf[MTU], waveRcvBuf[MTU];
  ssize_t rxlen = 0;

  struct sockaddr_in sndrAddr;
  size_t sndrAddrLen = sizeof(sndrAddr);
  size_t clickCliAddrLen = sizeof(_clickCliAddr);

  // infinite read loop
  while (true){

    if ((rxlen = recvfrom(_waveSockFd, waveRcvBuf, MTU, /*flags*/ 0,
        (struct sockaddr *) &sndrAddr, &sndrAddrLen)) > 0){ // error!
  
      assert(rxlen <= MTU-6);
      assert(rxlen > 14); // must be more than the ethernet header itself!

      // if there is someone listening
      if (_isClickCliConn > 0){

        // check to see that we ourselves aren't the originator of the message
        // can happen with broadcast packets
        uint64_t srcMacInt = 0;
        std::memcpy(&srcMacInt, &waveRcvBuf[6], 6);

        if (srcMacInt == myWaveMacInt){ // ignore and move on!
          continue;
        }

        // copy length
        uint16_t totalPktLen = rxlen + 6;
        const uint16_t totalPktLenNet = htons(totalPktLen);
        std::memcpy(&rcvBuf[0], &totalPktLenNet, 2);
        
        // note that there are 4 useless bytes in here, they server to add some
        // padding so the application header is the same length (6 bytes) on
        // both directions

        // copy data contents
        std::memcpy(&rcvBuf[6], waveRcvBuf, rxlen);

        if (_writePdrLog){ // if enabled, let us write this RX to the pdr log
          
          // we don't want to log packets sent to broadcast address
          bool isBroadcast = true;
          for (int i=0; i < 6; i++){
            if (waveRcvBuf[i] != 0xff){
              isBroadcast = false;
            }
          }
          
          if (not isBroadcast){
            _pdrLog.logRx(srcMacInt);
          }
        }

#ifdef DEBUG
        // get sender and destination macs
        std::ostringstream strStream;
        // destination mac
        for (int i=0; i < 6; i++){
          const uint8_t byte = waveRcvBuf[i];
          strStream << std::hex << std::setfill('0') << \
              std::setw(2) << static_cast<unsigned>(byte);
          if (i < 5){
            strStream << ":";
          }
        }
        std::string dstMac = strStream.str();

        // source mac
        strStream.str("");
        strStream.clear();
        for (int i=0; i < 6; i++){
          const uint8_t byte = waveRcvBuf[i+6];
          strStream << std::hex << std::setfill('0') << \
              std::setw(2) << static_cast<unsigned>(byte);
          if (i < 5){
            strStream << ":";
          }
        }
        std::string srcMac = strStream.str();

        // pretty-print sender's ip address
        char *senderIpStr;
        if ((senderIpStr = inet_ntoa(sndrAddr.sin_addr)) != NULL){

          std::cout << "Wave RX " << std::dec << nRx++ << " " << rxlen << \
              "b from " << srcMac << " (" << senderIpStr << ") to " << dstMac \
              << std::endl;

        } else{ // unable to figure out sender's IP

          std::cout << "Wave RX " << std::dec << rxlen << "b from " << \
              srcMac << " (ip?) to " << dstMac << std::endl;
        }   
#endif

        if (sendto(_clickSockFd, rcvBuf, totalPktLen, 0 /*flags*/, \
            (struct sockaddr *) &_clickCliAddr, clickCliAddrLen) < 0){
          std::cerr << "receiver_thread(), sendto(client): " << \
              strerror(errno) << std::endl;
        }
      }
    } else if (errno != EAGAIN){
      std::cerr << "receiver_thread(), recvfrom(wave): " << strerror(errno) \
          << std::endl;
    }
  }

  return 0;
}


/**
 * Returns 0 on success, 1 on failure.
 */
int parseAndSendWave(uint8_t *pktBuf, const uint16_t pktLen,
                     std::map<uint64_t,std::string> &macIpMap,
                     const uint16_t wavePort){

  assert(pktLen >= 20); // 6 for our header plus 14 for the ethernet header

  // deserialize
  int idx=0;

  // packet length
  uint16_t plen;
  std::memcpy(&plen, &pktBuf[idx], 2);
  idx += 2;
  plen = ntohs(plen);
  assert(plen == pktLen); // otherwise something's very wrong!
    
  // channel, tx power, data rate and priority ignored (1 byte each)
  idx += 4;

  // packet contents
  const int conLen = pktLen-6;
  assert(conLen > 0);

  uint8_t sndBuf[MTU];
  assert (conLen <= MTU);
  std::memcpy(sndBuf, &pktBuf[idx], conLen);
  idx += conLen;
  assert(idx == pktLen);

  // figure out what IP the destination mac corresponds to
  uint64_t dstMacInt = 0;
  std::memcpy(&dstMacInt, &sndBuf, 6);

  if (macIpMap.find(dstMacInt) == macIpMap.end()){

    std::cerr << "Can't find IP for destination mac ";

    for (int i=0; i < 6; i++){
      const uint8_t byte = sndBuf[i];
      std::cerr << std::hex << std::setfill('0') << \
          std::setw(2) << static_cast<unsigned>(byte);
      if (i < 5){
        std::cout << ":";
      }
    }
    std::cerr << ". Dropping message." << std::endl;

    return -1;
  }

  std::string dstIpStr = macIpMap[dstMacInt];

  struct sockaddr_in dstAddr;
  memset(&dstAddr, 0, sizeof(dstAddr));
  dstAddr.sin_family = AF_INET;
  dstAddr.sin_port = htons(wavePort);

  if (inet_aton(dstIpStr.c_str(), &dstAddr.sin_addr) == 0){ // invalid address
  
    std::cerr << "Can't convert destination IP " << dstIpStr << \
      " into network address. Dropping message." << std::endl;
    return -1;
  }

  // send UDP packet
  
  if (_writePdrLog){ // if enabled, let us write this TX to the pdr log
    
    bool isBroadcast = true;
    for (int i=0; i < 6; i++){
      if (sndBuf[i] != 0xff){
        isBroadcast = false;
      }
    }
    
    if (not isBroadcast){
      uint64_t dstMacInt = 0;
      std::memcpy(&dstMacInt, sndBuf, 6);
      _pdrLog.logTx(dstMacInt);
    }
  }
  
#ifdef DEBUG
  std::cout << "Wave TX " << std::dec << nTx++ << " " << conLen << "b from ";

  for (int i=0; i < 6; i++){
    const uint8_t byte = sndBuf[i+6]; // src mac
    std::cout << std::hex << std::setfill('0') << \
        std::setw(2) << static_cast<unsigned>(byte);
    if (i < 5){
      std::cout << ":";
    }
  }

  std::cout << " to ";
  for (int i=0; i < 6; i++){
    const uint8_t byte = sndBuf[i];
    std::cout << std::hex << std::setfill('0') << \
        std::setw(2) << static_cast<unsigned>(byte);
    if (i < 5){
      std::cout << ":";
    }
  }
  
  std::cout << " (" << dstIpStr << ")" << std::endl;
#endif

  // the packet is now fully assembled and ready to be sent
  if (sendto(_waveSockFd, sndBuf, conLen, 0 /*flags*/, \
      (struct sockaddr *) &dstAddr, sizeof(dstAddr)) < 0) {
    std::cerr << "handleClient(), sendto(wave): " << strerror(errno) << \
        std::endl;
  }

  return 0;
}

/**
 * This thread periodically writes and resets the PDR log. In between it just
 * sleeps.
 */
void *logger_thread(void *arg){

  // read the arguments
  LgrThdArgs *threadArgs = (LgrThdArgs *) arg;
  
  const std::string& logFname = threadArgs->logFname;
  
  // let us open up the log file
  std::ofstream ofs;
  ofs.open (logFname, std::ios::out | std::ios::app);
  
  if (!ofs.is_open()){ // failure?
    std::cerr << "logger_thread() couldn't open pdr log file " << logFname << \
        ". No PDR log will be created." << std::endl;
    return NULL; // nothing for us here now
  }

  // if we're starting on a brand new file, let us write a little header for it
  ofs.seekp(0, std::ios::end); // put cursor at eof
  if (ofs.tellp() == 0){
    const std::string& myWaveMacStr = threadArgs->myWaveMacStr;
    ofs << "# PDR log for MAC " << myWaveMacStr << std::endl << "# Format:" \
        << std::endl << "# tstamp mac ntx nrx" << std::endl;
  }

  PdrLog& pdrLog = threadArgs->pdrLog; // the log we shall be writing to

  // configure log periodicity
  const unsigned long timeres = threadArgs->timeres;
  sysclock::time_point nextTimepoint = sysclock::from_time_t(0);

  while(true){
  
    std::this_thread::sleep_until(nextTimepoint); // wait for the right time
    
    // take note of current time with desired granularity
    sysclock::time_point nowTimepoint = sysclock::now();
    const secs nowTstampSecs = \
        std::chrono::duration_cast<secs> (nowTimepoint.time_since_epoch());
    const std::time_t nowTstamp = nowTstampSecs.count() / timeres * timeres;

    pdrLog.print(ofs, nowTstamp, true /* reset */); // do the actual print
    ofs.flush(); // write it all out
    
    // how long should we sleep for?
    const std::time_t nextTstamp = nowTstamp + timeres;
    nextTimepoint = sysclock::from_time_t(nextTstamp);
  }
  
  ofs.close();

  return 0; // unreachable code!
}


/**
 * Runs the show.
 */
int main(int argc, char *argv[]){

  // 1. CONFIGURATION

  // config filename can be passed as argument, otherwise use default
  std::ostringstream cfgFnameStream;
  if (argc >= 2){
      cfgFnameStream << argv[1];
  } else {
      cfgFnameStream << argv[0] << ".conf";
  }

  // check to see if the configuration file exists
  std::ifstream cfgfile(cfgFnameStream.str().c_str(), std::ifstream::in);
  if (not cfgfile.good()){
      std::cerr << "could not open config file \"" << cfgFnameStream.str() \
          << "\" will use defaults." << std::endl;
  }

  // b. process configuration file
  ConfigFile conf(cfgFnameStream.str());

  // server port
  uint16_t pClickPort = 45622;
  try{
    const unsigned port = (unsigned int) \
           std::atol(conf.Value("server", "port").c_str());

    if (port < 1024 or port > 65535){
      throw "port must be in [1024,65535]";
    }

    pClickPort = port;

  } catch (const std::string str) {
    std::cerr << "Config error: section=server, value=port (" << str \
        << "), using default value " << pClickPort << "." << std::endl;
  }

  // server mac->ip map filename
  std::string pMacIpMapFname = "mac-ip-map.conf";
  try{
    pMacIpMapFname = conf.Value("server", "macipmapfname");

  } catch (const std::string str) {
    std::cerr << "Config error: section=server, value=macipmapfname (" << str \
        << "), using default value " << pMacIpMapFname << "." << std::endl;
  }

  // wave mac
  std::string pMyWaveMacStr = "";
  try {
    pMyWaveMacStr = conf.Value("wave", "mac");
  } catch (const std::string str){
    std::cerr << "Config error: section=wave, value=mac (" << str \
          << ") not found. Exiting." << std::endl;
    die();
  }
  uint64_t myWaveMacInt;
  if (getMacIntFromStr(pMyWaveMacStr, myWaveMacInt) < 0){
    std::cerr << "Invalid wave mac " << pMyWaveMacStr << ". Exiting." << std::endl;
    die();
  }

  // wave port
  uint16_t pWavePort = 45623;
  try {
    const unsigned port = (unsigned int) \
            std::atol(conf.Value("wave", "port").c_str());

     if (port < 1024 or port > 65535){
       throw "port must be in [1024,65535]";
     }

     pWavePort = port;

  } catch (const std::string str){
    std::cerr << "Config error: section=wave, value=port (" << str \
          << "), using default value " << unsigned(pWavePort) << "." << \
          std::endl;
  }

  // check to see if the file exists
  std::ifstream mapFile(pMacIpMapFname.c_str(), std::ifstream::in);
  if (not mapFile.good()){
    std::cerr << "could not open mac->ip map file \"" << pMacIpMapFname \
        << "\". Exiting." << std::endl;
    die();
  }

  // load the map
  std::map<uint64_t,std::string> macIpMap;
  loadMacIpMap(pMacIpMapFname, macIpMap); // actual load done here

#ifdef DEBUG
  std::cout << "Loaded " << macIpMap.size() << " MAC->IP pairs" << std::endl;
#endif

  // pdr logger
  _writePdrLog = false;
  try {
    _writePdrLog = (bool) std::atoi(conf.Value("pdrlog", "writelog").c_str());
  } catch (const std::string str){
    std::cerr << "Config error: section=pdrlog, value=writelog (" << str \
          << "). PDR log disabled." << std::endl;
  }
  
  // additional processing to be done only if PDR logging is turned on
  if (_writePdrLog){

    // read time resolution
    std::time_t timeres = 1; // default seconds
    try {
      timeres = \
          (std::time_t) std::atol(conf.Value("pdrlog", "timeres").c_str());

    } catch (const std::string str){
      std::cerr << "Config error: section=pdrlog, value=timeres (" << str \
          << "), using default " << timeres << "." << std::endl;
    }
    
    // read log file name
    std::string logFname = "pdrlog.txt";
    try {
      logFname = conf.Value("pdrlog", "fname");

    } catch (const std::string str){
      std::cerr << "Config error: section=pdrlog, value=fname (" << str \
          << "), using default " << logFname << "." << std::endl;
    }
    
    // initialize the arg struct
    // can only do it now because we can't have uninitialized references
    LgrThdArgs lgrThdArgs = {
      _pdrLog,  // pdrLog
      timeres,  // timeres
      logFname,  // logFname
      pMyWaveMacStr,  // myWaveMacStr
    };

    // finally create the thread
    if (pthread_create(&_loggerTid, NULL, logger_thread, &lgrThdArgs)){
      std::cerr << "main(), pthread_create(&_loggerTid): " << \
        strerror(errno) << std::endl;
      die();
    }
  }

  // 3. CREATE WAVE (UDP) SOCKET
  // create the socket
  if ((_waveSockFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
    std::cerr << "main(), socket(wave): " << strerror(errno) << std::endl;
    die();
  }

  // set reuseaddr option
  const int isReuseaddr = 1; // true
  if (setsockopt(_waveSockFd, SOL_SOCKET, SO_REUSEADDR, &isReuseaddr, \
      sizeof(isReuseaddr)) == -1){
    std::cerr << "main(), setsockopt(wave, reuseaddr): " << strerror(errno) << \
        std::endl;
    die();
  }

  // set broadcast option (needed to receive broadcast
  const int broadcast = 1; // true
  if (setsockopt(_waveSockFd, SOL_SOCKET, SO_BROADCAST, &broadcast, \
      sizeof(broadcast)) == -1){
    std::cerr << "main(), setsockopt(wave, broadcast): " << strerror(errno) << \
        std::endl;
    die();
  }

  // set TTL option to 1 so broadcast packets don't go on forever
  const int ttl = 1;
  if (setsockopt(_waveSockFd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) == -1){
    std::cerr << "main(), setsockopt(wave, ttl): " << strerror(errno) << \
        std::endl;
    die();
  }

  // let's bind to INADDR_ANY so we can get packets from all interfaces
  struct sockaddr_in waveAddr; /* wave's addr */
  memset(&waveAddr, 0, sizeof(waveAddr));
  waveAddr.sin_family = AF_INET;
  waveAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  waveAddr.sin_port = htons(pWavePort);

  // create the (wave) socket
  if (bind(_waveSockFd, (struct sockaddr *) &waveAddr, sizeof(waveAddr)) < 0){
    std::cerr << "main(), bind(wave): " << strerror(errno) << std::endl;
    die();
  }

  // 4. LAUNCH RECEIVER THREAD
  RcvThdArgs rcvThdArgs;
  rcvThdArgs.myWaveMacInt = myWaveMacInt;
  if (pthread_create(&_rcvrTid, NULL, receiver_thread, &rcvThdArgs)){
    std::cerr << "main(), pthread_create(&_rcvrTid): " << \
        strerror(errno) << std::endl;
    die();
  }

  // 5. SIGNAL HANDLING SET UP
  // set up termination signal handler
  signal(SIGINT, sigHandler);
  signal(SIGTERM, sigHandler);
  signal(SIGHUP, sigHandler);

  // 6. BRING CLICK (UDP) SERVER UP

  struct sockaddr_in clickSrvAddr; /* click's addr */
  memset(&clickSrvAddr, 0, sizeof(clickSrvAddr));
  clickSrvAddr.sin_family = AF_INET;
  clickSrvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  clickSrvAddr.sin_port = htons(pClickPort);
  
  // create the socket
  if ((_clickSockFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
    std::cerr << "main(), socket(click): " << strerror(errno) << std::endl;
    die();
  }
  // set reuseaddr option
  // isReuseaddr was previously defined to be true
  if (setsockopt(_clickSockFd, SOL_SOCKET, SO_REUSEADDR, &isReuseaddr, \
      sizeof(isReuseaddr)) == -1){
    std::cerr << "main(), setsockopt(click, reuseaddr): " << strerror(errno) \
        << std::endl;
    die();
  }

  // bind the (click) socket
  if (bind(_clickSockFd, (struct sockaddr *) &clickSrvAddr, \
      sizeof(clickSrvAddr)) < 0){
    std::cerr << "main(), bind(click): " << strerror(errno) << std::endl;
    die();
  }

#ifdef DEBUG
  std::cout << "Server up and listening" << std::endl;
#endif

  // 6. MAIN LISTEN LOOP
  uint8_t clickRcvBuf[MTU];
  size_t rxlen = 0;

  size_t clickCliAddrLen = sizeof(_clickCliAddr);

  // infinite read loop (until death due us part)
  while (true){

    if ((rxlen = recvfrom(_clickSockFd, clickRcvBuf, MTU, 0 /*flags*/,
        (struct sockaddr *) &_clickCliAddr, &clickCliAddrLen)) > 0){

      // process connect/disconnect codes
      if (rxlen == 1){
      
        switch (clickRcvBuf[0]){ // special sync codes 
          case CLICK_CON_CODE:
            _isClickCliConn = true;
            break;
          case CLICK_DISC_CODE:
            _isClickCliConn = false;
            break;
          default:
            break;
        }
      } else{
      
        assert(rxlen >= 20); // 6 for our header plus 14 for the ethernet header
        // 1 datagram = 1 wave message, so send it out!
        parseAndSendWave(clickRcvBuf, rxlen, macIpMap, pWavePort);
      }
    }
  }

  die(); // unreacheable but oh well

  return 0;
}

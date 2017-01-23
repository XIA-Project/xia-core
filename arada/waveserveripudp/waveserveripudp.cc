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

// legacy c standard headers
#include <cmath> // pow()
#include <csignal> // signal()
#include <cstdlib> // strtoul()
#include <cstdint> // uint*_t
#include <cstring> // strerror(), memcpy(), memset()
#include <cassert> // assert()
#include <ctime> // time()

// unix headers
#include <unistd.h> // getpid()
#include <pthread.h> // pthread_*()
#include <arpa/inet.h> // htons(), ntohs(), etc
#include <sys/socket.h> // socket(), setsockopt(), bind(), ...
//#include <sys/types.h> // accept(), freeaddrinfo()
#include <errno.h> // errno

// my headers
#include "configfile.h"

// Global definitions
#define MTU 66000 // let's support 64K jumbo packets, just in case...

#define CLICK_CON_CODE 254 // click connect code, so we know someone's listening
#define CLICK_DISC_CODE 255 // click disconnect code, so we know it's gone

/* Global variables, aren't they fun */
pthread_t _rcvrTid=0;
int _clickSockFd=0, _waveSockFd=0;
struct sockaddr_in _clickCliAddr;
bool _isClickCliConn = false;

struct RcvThdArgs {
	uint64_t myWaveMacInt;
};

// debug
unsigned long long nTx=0, nRx=0;


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

  macInt = 0;
  int exp = 0;
  for (std::vector<std::string>::reverse_iterator itr = macSplinters.rbegin();
      itr != macSplinters.rend(); ++itr) {

    const uint8_t macByte = strtoul(itr->c_str(), NULL, 16);
    macInt += (uint64_t) macByte * pow(16, exp);
    exp += 2;
  }

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
        int exp = 0;
        for (int i=5; i >= 0; i--){
          const uint8_t macByte = waveRcvBuf[i+6]; // src mac is 2nd in line
          srcMacInt += (uint64_t) macByte * pow(16, exp);
          exp += 2;
        }

        if (srcMacInt == myWaveMacInt){ // ignore and move on!
          continue;
        }

        // copy length
        uint16_t totalPktLen = rxlen + 6;
        const uint16_t totalPktLenNet = htons(totalPktLen);
        memcpy(&rcvBuf[0], &totalPktLenNet, 2);
        
        // note that there are 4 useless bytes in here, they server to add some
        // padding so the application header is the same length (6 bytes) on
        // both directions

        // copy data contents
        memcpy(&rcvBuf[6], waveRcvBuf, rxlen);        

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

          std::cout << "Wave RX " << std::dec << nRx++ << " " << rxlen << "b from " << \
              srcMac << " (" << senderIpStr << ") to " << dstMac << std::endl;

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
  memcpy(&plen, &pktBuf[idx], 2);
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
  memcpy(sndBuf, &pktBuf[idx], conLen);
  idx += conLen;
  assert(idx == pktLen);

  // figure out what IP the destination mac corresponds to
  uint64_t dstMacInt = 0;
  int exp = 0;
  for (int i=5; i >= 0; i--){
    const uint8_t macByte = sndBuf[i]; // destination mac is at the beginning
    dstMacInt += (uint64_t) macByte * pow(16, exp);
    exp += 2;
  }

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

#ifdef DEBUG
if (conLen > 350){
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
}
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
  std::string pMyWaveMac = "";
  try {
    pMyWaveMac = conf.Value("wave", "mac");
  } catch (const std::string str){
    std::cerr << "Config error: section=wave, value=mac (" << str \
          << ") not found. Exiting." << std::endl;
    die();
  }
  uint64_t myWaveMacInt;
  if (getMacIntFromStr(pMyWaveMac, myWaveMacInt) < 0){
    std::cerr << "Invalid wave mac " << pMyWaveMac << ". Exiting." << std::endl;
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
    std::cerr << "main(), setsockopt(click, reuseaddr): " << strerror(errno) << \
        std::endl;
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

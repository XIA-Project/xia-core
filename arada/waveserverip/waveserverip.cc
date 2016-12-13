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
#include <chrono> // std::chrono
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

// unix headers
#include <unistd.h> // getpid()
#include <pthread.h> // pthread_*()
#include <arpa/inet.h> // htons(), ntohs(), etc
#include <sys/socket.h> // socket(), getaddrinfo(), setsockopt(), bind(), ...
#include <sys/types.h> // accept(), getaddrinfo(), freeaddrinfo()
#include <netdb.h> // getaddrinfo(), freeaddrinfo()
#include <errno.h> // errno

// my headers
#include "configfile.h"

// Global definitions
#define MTU 66000

/* Global variables, aren't they fun */
pthread_t _rcvrTid=0;
int _clickServSockFd=0, _clickCliSockFd=0, _waveSockFd=0;

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

  // close the click client socket, if one exists
  if (_clickCliSockFd > 0){
    if (close(_clickCliSockFd) == -1){
      std::cerr << "die(), close(_clickCliSockFd): " << strerror(errno) << std::endl;
    }
  }

  // close the click server socket
  if (_clickServSockFd != 0){
    if (close(_clickServSockFd) == -1){
      std::cerr << "die(), close(_clickServSockFd): " << strerror(errno) << \
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

  uint8_t rcvBuf[MTU], waveRcvBuf[MTU];
  ssize_t rxlen = 0;

  struct sockaddr_in sndrAddr;
  unsigned int sndrAddrLen = sizeof(sndrAddr);

  assert(_waveSockFd != 0); // the socket must be ready by thread launch!

  // infinite read loop
  while (true){

    if ((rxlen = recvfrom(_waveSockFd, waveRcvBuf, MTU, /*flags*/ 0,
        (struct sockaddr *) &sndrAddr, &sndrAddrLen)) > 0){ // error!

      assert (rxlen > 0); // otherwise we wouldn't be here

      // if there is someone listening
      if (_clickCliSockFd > 0){

        assert(rxlen <= MTU-2);
        assert(rxlen > 14); // must be more than the ethernet header itself!

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
        uint16_t totalPktLen = rxlen + 2;
        const uint16_t totalPktLenNet = htons(totalPktLen);
        memcpy(&rcvBuf[0], &totalPktLenNet, 2);

        // copy data contents
        memcpy(&rcvBuf[2], waveRcvBuf, rxlen);

#ifdef DEBUG
if (rxlen > 350){
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
}        
#endif

        if (write(_clickCliSockFd, rcvBuf, totalPktLen) < 0){
          std::cerr << "receiver_thread(), write(): " << strerror(errno) << \
              std::endl;
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
  if (sendto(_waveSockFd, sndBuf, conLen, /*flags*/ 0, \
      (struct sockaddr *) &dstAddr, sizeof(dstAddr)) < 0) {
    std::cerr << "handleClient(), sendto(wave): " << strerror(errno) << \
        std::endl;
  }

  return 0;
}


/**
 * Process requests from a client for as long as he remains connected.
 */
void handleClient(std::map<uint64_t,std::string> &macIpMap, 
                  const uint16_t wavePort){

  // reception helpers
  uint8_t rcvBuf[MTU];
  uint8_t pktBuf[MTU];
  uint16_t pktLen=0, pktBufIdx=0;

  int nrcvd;
  while (true){

    if((nrcvd = recv(_clickCliSockFd, rcvBuf, MTU, 0 /*flags*/)) > 0){
        
      // got something to work with
      int rcvBufIdx = 0;
      int nLeft = nrcvd;
            
      while (nLeft > 0){ // meaning there is still stuff to consume

        if (pktLen == 0) { // at the beginning of a packet

          if ((pktBufIdx + nLeft) >= 2){ // have enough to fill pktLen

            // copy to packet buffer
            assert(pktBufIdx < 2);
            const int ncpy = 2-pktBufIdx;
            memcpy(&pktBuf[pktBufIdx], &rcvBuf[rcvBufIdx], ncpy);

            // update state variables
            nLeft -= ncpy;
            pktBufIdx += ncpy;
            rcvBufIdx += ncpy;
            assert(nLeft >= 0);
            assert(rcvBufIdx <= nrcvd);

            // now set packet length
            memcpy(&pktLen, pktBuf, 2); // copy to pktLen
            pktLen = ntohs(pktLen); // back to host order

          } else { // don't have enough to fill packet length

            assert(pktLen == 0 and nLeft == 1); // ensuring sanity
            assert(pktBufIdx == 0);

            // copy a single byte
            const int ncpy = nLeft;
            memcpy(&pktBuf[pktBufIdx], &rcvBuf[rcvBufIdx], ncpy);
            pktBufIdx += ncpy;
            rcvBufIdx += ncpy;
            nLeft -= ncpy;
                        
            assert(nLeft == 0);
          }
        } else{ // in the middle of a packet

          assert(pktLen > 0); // the packet length is known

          // let us copy as much as we can
          const int nmissing = pktLen-pktBufIdx;
          const int ncpy = nmissing < nLeft ? nmissing : nLeft;

          memcpy(&pktBuf[pktBufIdx], &rcvBuf[rcvBufIdx], ncpy);

          // update state variables
          nLeft -= ncpy;
          pktBufIdx += ncpy;
          rcvBufIdx += ncpy;
          assert(nLeft >= 0);
          assert(rcvBufIdx <= nrcvd);

          if (nmissing == ncpy){ // got complete packet, send it out!
            assert(pktBufIdx == pktLen);

            // parse and send it out the air interface
            parseAndSendWave(pktBuf, pktLen, macIpMap, wavePort);

            // reset the packet buffer state
            pktLen = 0;
            pktBufIdx = 0;
          }
        }
      }
    } else if (nrcvd == 0){ // means client disconnected
      break; // exit method and
    } else{ // means nrcvd < 0
      std::cerr << "handleClient(), recv(clickCli): " << strerror(errno) << \
          std::endl;
    }
  }
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
  std::string pPort = "45622";
  try{
    const unsigned port = (unsigned int) \
           std::atol(conf.Value("server", "port").c_str());

    if (port < 1024 or port > 65535){
      throw "port must be in [1024,65535]";
    }

    pPort = conf.Value("server", "port");

  } catch (const std::string str) {
    std::cerr << "Config error: section=server, value=port (" << str \
        << "), using default value " << pPort << "." << std::endl;
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
  const int reuseaddr = 1; // true
  if (setsockopt(_waveSockFd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, \
      sizeof(reuseaddr)) == -1){
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


  // 6. BRING CLICK (TCP) SERVER UP

  // get the address info
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if (getaddrinfo(NULL, pPort.c_str(), &hints, &res) != 0){
    std::cerr << "main(), getaddrinfo(click): " << strerror(errno) << std::endl;
    die();
  }

  // create the (server) socket
  if ((_clickServSockFd = socket(res->ai_family, res->ai_socktype, \
      res->ai_protocol)) == -1){
    std::cerr << "main(), socket(clickServ): " << strerror(errno) << std::endl;
    die();
  }

  // enable the socket to reuse the address
  if (setsockopt(_clickServSockFd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, \
      sizeof(reuseaddr)) == -1){
    std::cerr << "main(), setsockopt(clickServ, reuseaddr): " << \
        strerror(errno) << std::endl;
    die();
  }

  // bind to the address
  if (bind(_clickServSockFd, res->ai_addr, res->ai_addrlen) == -1){
    std::cerr << "main(), bind(clickServ): " << strerror(errno) << std::endl;
    die();
  }

  // listen on the socket
  if (listen(_clickServSockFd, 0 /*backlog*/) == -1){
    std::cerr << "main(), listen(clickServ): " << strerror(errno) << std::endl;
    die();
  }

  freeaddrinfo(res);

#ifdef DEBUG
  std::cout << "Server up and listening" << std::endl;
#endif

  // 6. MAIN LISTEN LOOP
    
  while (true) {
    
    struct sockaddr_in clientAddr;
    size_t sockaddrLen = sizeof(clientAddr);

    if ((_clickCliSockFd = accept(_clickServSockFd, \
        (struct sockaddr*)&clientAddr, &sockaddrLen)) == -1){ // failure
        
      std::cerr << "main(), accept(clickCli): " << strerror(errno) << std::endl;
            
    } else{ // success!

#ifdef DEBUG
      std::cout << "Got a client from " << \
          inet_ntoa(clientAddr.sin_addr) << " on port " << \
          htons(clientAddr.sin_port) << std::endl;
#endif
      handleClient(macIpMap, pWavePort);
      _clickCliSockFd = 0;
           
#ifdef DEBUG
      std::cout << "Client disconnected" << std::endl;
#endif
    }
  }
    
  die();

  return 0;
}

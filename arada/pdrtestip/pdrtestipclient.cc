/**
 * TCP server that provides a simple tx/rx interface to remote nodes.
 * Packets are sent using UDP/IP over WAVE.
 * Only supports a single client at a time.
 *
 * Rui Meireles (rui@cmu.edu)
 * Summer 2016
 */

// c++ standard headers
#include <iostream> // std::cout, std::cerr
#include <iomanip> // std::setprecision()

// legacy c standard headers
#include <csignal> // signal()
#include <cstdint> // uint*_t
#include <cstring> // strerror(), memcpy(), memset()
#include <cstdlib> // atoi(), exit()
#include <ctime> // time()

// unix headers
#include <unistd.h> // close(), usleep()ee
#include <arpa/inet.h> // htons(), ntohs(), etc
#include <sys/socket.h> // socket(), setsockopt(), bind(), ...
#include <errno.h> // errno

// Global definitions
#define MTU 65536

// Global variables, aren't they fun
int _waveSockFd = 0;
bool _verbose = true;
int _wavePort = 45624;
int _pSize = 256; // packet size
int _burstLen = 1; // number of consecutive packets
int _sleepTime = 0; // in microsecs
std::string _dstIpStr = "192.168.3.101";


/**
 * Kills the program with all the poise and grace of a harakiri.
 */
void die(){

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


/**
 * display cmd line options and exit
 */
void help(){

  std::cout << "usage: pdrtestipclient [-q] [-p port] [-s size] [-b burst] \
[-i sleep_interval] [-d dst_ip] " << std::endl << \
    "where:" << std::endl << \
	" -q : quiet mode (no output)" << std::endl << \
   	" -p : wave port (default " << _wavePort << ")" << std::endl << \
   	" -s : packet size (default " << _pSize << ")" << std::endl << \
   	" -b : burst length (default " << _burstLen << ")" << std::endl << \
   	" -i : sleep between bursts (usec, default " << _sleepTime << ")" << \
    std::endl << \
    " -d : destination ip (default " << _dstIpStr << ")" << std::endl;
  exit(0);
}


/**
 * configure the app
 */
void getConfig(int argc, char** argv){

  int c;
  opterr = 0;

  while ((c = getopt(argc, argv, "hqp:s:b:i:d:")) != -1){
    
    switch (c){
      case '?':
      case 'h':
        // Help Me!
        help();
        break;
      case 'q':
        // turn on quiet mode
        _verbose = false;
        break;
      case 'p':
        // use specified port
        _wavePort = atoi(optarg);
        break;
      case 's':
        _pSize = atoi(optarg);
        if (_pSize > MTU){
          _pSize = MTU;
        }
        break;
      case 'b':
        _burstLen = atoi(optarg);
        break;
      case 'i':
        _sleepTime = atoi(optarg);
        break;
      case 'd':
        _dstIpStr = optarg;
        break;
      default:
        help();
        break;
    }
  }
}


/**
 * Runs the show.
 */
int main(int argc, char *argv[]){

  // 1. CONFIGURATION
  getConfig(argc, argv);

  // 2. CREATE WAVE (UDP) SOCKET
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
  waveAddr.sin_port = htons(_wavePort);

  // bind the socket to the specified port
  if (bind(_waveSockFd, (struct sockaddr *) &waveAddr, sizeof(waveAddr)) < 0){
    std::cerr << "main(), bind(wave): " << strerror(errno) << std::endl;
    die();
  }
  
  // 3. SET UP SIGNAL HANDLER
  // set up termination signal handler
  signal(SIGINT, sigHandler);
  signal(SIGTERM, sigHandler);
  signal(SIGHUP, sigHandler);


  // 4. SET UP DST ADDR
  
  struct sockaddr_in dstAddr;
  memset(&dstAddr, 0, sizeof(dstAddr));
  dstAddr.sin_family = AF_INET;
  dstAddr.sin_port = htons(_wavePort);

  if (inet_aton(_dstIpStr.c_str(), &dstAddr.sin_addr) == 0){ // invalid address
  
    std::cerr << "Can't convert destination IP " << _dstIpStr << \
      " into network address. Dropping message." << std::endl;
    
    die();
  }

  // 5. SENDING LOOP
  uint8_t sndBuf[MTU];
  unsigned long long nTx=0, nBytes=0;
  time_t prevTime = time(NULL);
  double mbits = 0;

  // configure output format
  std::cout << std::setiosflags(std::ios::fixed) << std::setprecision(3);

  // infinite read loop
  while (true){

    // do a burst
    for (int i=0; i < _burstLen; i++){

      // the packet is now fully assembled and ready to be sent
      if (sendto(_waveSockFd, sndBuf, _pSize, 0 /*flags*/, \
        (struct sockaddr *) &dstAddr, sizeof(dstAddr)) < 0) {

        std::cerr << "error, sendto(wave): " << strerror(errno) << \
          std::endl;
      }

      nTx++;
      nBytes += _pSize;
      
      if (_verbose){

        // time to print out throughput info?
        time_t nowTime = time(NULL);
        if (nowTime > prevTime){ // changing second

          mbits = ((double)nBytes)*8/1000000/(nowTime-prevTime);
          
          std::cout << "TX #" << nTx << ", " << mbits << " Mbps" << std::endl;

          // reset state
          prevTime = nowTime;
          nBytes = 0;
        }    
      }
    }

    // sleep
    usleep(_sleepTime);
  }

  die(); // unreacheable but oh well

  return 0;
}

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

// legacy c standard headers
#include <csignal> // signal()
#include <cstdint> // uint*_t
#include <cstring> // strerror(), memcpy(), memset()
#include <cstdlib> // atoi(), exit()
#include <ctime> // time()

// unix headers
#include <unistd.h> // close()
#include <arpa/inet.h> // htons(), ntohs(), etc
#include <sys/socket.h> // socket(), setsockopt(), bind(), ...
#include <errno.h> // errno

// Global definitions
#define MTU 65536

// Global variables, aren't they fun
int _waveSockFd = 0;
bool _verbose = true;
int _wavePort = 45624;


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

  std::cout << "usage: pdrtestipserver [-q] [-p]" << std::endl << \
    "where:" << std::endl << \
	" -q : quiet mode (no output)" << std::endl << \
   	" -p : wave port (default " << _wavePort << ")" << std::endl;
  exit(0);
}


/**
 * configure the app
 */
void getConfig(int argc, char** argv){

  int c;
  opterr = 0;

  while ((c = getopt(argc, argv, "hqp:")) != -1){
    
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


  // 4. RECEIVING LOOP
  uint8_t waveRcvBuf[MTU];
  ssize_t rxlen = 0;

  struct sockaddr_in sndrAddr;
  size_t sndrAddrLen = sizeof(sndrAddr);

  unsigned long long nRx = 0;
  time_t prevTime = time(NULL);

  // infinite read loop
  while (true){

    if ((rxlen = recvfrom(_waveSockFd, waveRcvBuf, MTU, /*flags*/ 0,
        (struct sockaddr *) &sndrAddr, &sndrAddrLen)) > 0){ // error!

      nRx++;

      if (_verbose){

        time_t nowTime = time(NULL);

        if (nowTime > prevTime){
          std::cout << "RX #" << nRx << std::endl;
          prevTime = nowTime; // reset state
        }
      }

    } else if (errno != EAGAIN){
      std::cerr << "ERROR recvfrom(wave): " << strerror(errno) << std::endl;
    }
  }

  die(); // unreacheable but oh well

  return 0;
}

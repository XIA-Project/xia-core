/**
 * TCP server that provides a simple tx/rx WAVE interface to remote nodes.
 * Only supports a single client at a time.
 *
 * Rui Meireles (rui@cmu.edu)
 * Spring 2016
 */

// c++ standard headers
#include <string> // std::string
#include <fstream> // std::ifstream
#include <iostream> // std::cout, std::cerr
#include <sstream> // std::ostringstream
#include <chrono> // std::chrono
#include <thread> // std::this_thread::sleep_for()
#include <iomanip> // std::hex, etc

// legacy c standard headers
#include <csignal> // signal()
#include <cstdlib> // std::atoi()
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

// arada headers
extern "C" {
    #include <arada/wave.h>
}

// my headers
#include "configfile.h"

/* Global variables, aren't they fun */

enum {WAVE_PROVIDER, WAVE_USER}; // WAVE roles

bool _waveReg=false;
int _waveRole = WAVE_PROVIDER;
pid_t _pid=0;
pthread_t _rcvrTid=0;
int _servSockFd=0, _cliSockFd=0;
uint32_t _pPsid = 5;

static WMEApplicationRequest _wmeReq;

unsigned long long nTx=0, nRx=0;

/**
 * Kills the program the poise and grace of a harakiri.
 */
void die(){

    // cancel the receiver thread (not joined because infinite loop)
    if (_waveReg){
        if (_waveRole == WAVE_PROVIDER)
            removeProvider(_pid, &_wmeReq);
        else{
            removeUser(_pid, &_wmeReq);
            assert(_waveRole == WAVE_USER); // no other option, really
        }
    }

    // cancel the receiver thread (not joined because infinite loop)
    if (_rcvrTid != 0 and pthread_cancel(_rcvrTid)){
        std::cerr << "die(), pthread_cancel(_rcvrTid): " << strerror(errno) << \
            std::endl;
    }

    // close the client socket, if one exists
    if (_cliSockFd > 0){
        if (close(_cliSockFd) == -1){
            std::cerr << "die(), close(_cliSockFd): " << strerror(errno) << \
                std::endl;
        }
    }

    // close the server socket
    if (_servSockFd != 0){
        if (close(_servSockFd) == -1){
            std::cerr << "die(), close(_servSockFd): " << strerror(errno) << \
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


void* receiver_thread(void */*arg*/){

    // this is an infinite-loop type thread
    // join() won't work so enable cancellation
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    // prepare wsm indication data structure
    WSMIndication rxPkt;

    uint8_t rcvBuf[FOURK];

    // infinite read loop
    while (true){

        if (rxWSMPacket(_pid, &rxPkt) > 0){ // got a packet, yay

            // if payload and there is someone listening
            if (rxPkt.data.length > 0 and _cliSockFd > 0){

                assert(rxPkt.data.length < FOURK);

                // copy length
                uint16_t totalPktLen = rxPkt.data.length+2;
                const uint16_t totalPktLenNet = htons(totalPktLen);
                
                memcpy(&rcvBuf[0], &totalPktLenNet, 2);
                // copy data contents 
                memcpy(&rcvBuf[2], &rxPkt.data.contents, rxPkt.data.length);

#ifdef DEBUG
if (rxPkt.data.length > 400){
                std::cout << "Received wsm " << std::dec << nRx++ << ", " << std::dec << rxPkt.data.length \
                << " bytes, from mac: ";

                for (int i=0; i < IEEE80211_ADDR_LEN; i++){
                    const uint8_t byte = rxPkt.macaddr[i];
                    std::cout << std::hex << std::setfill('0') << \
                    std::setw(2) << static_cast<unsigned>(byte);
                    if (i < (IEEE80211_ADDR_LEN-1)){
                        std::cout << ":";
                    }
                }
                std::cout << std::endl;
}
#endif

                if (write(_cliSockFd, rcvBuf, totalPktLen) < 0){
                    std::cerr << "receiver_thread(), write(): " << \
                        strerror(errno) << std::endl;
                }
            }
        } else if (errno != EAGAIN){
            std::cerr << "receiver_thread(), rxWSMPacket(): " << \
                strerror(errno) << std::endl;
        }
    }

    return 0;
}


void parseAndSendWSM(uint8_t *pktBuf, uint16_t pktLen){

    // a) build the WSM request packet
    static WSMRequest wsmReq; // has to be static for unknown reasons!
    wsmReq.version = 1;
    wsmReq.security = 0;
    wsmReq.psid = _pPsid; // mandatory, must match registration

    int idx=0;
    // deserialize

    // packet length
    uint16_t plen;
    memcpy(&plen, &pktBuf[idx], 2);
    idx += 2;
    plen = ntohs(plen);
    assert(plen == pktLen); // otherwise something's very wrong!
    
    // channel
    memcpy(&wsmReq.chaninfo.channel, &pktBuf[idx], 1);
    idx += 1;

    // txpower 
    memcpy(&wsmReq.chaninfo.txpower, &pktBuf[idx], 1);
    idx += 1;

    // data rate index
    memcpy(&wsmReq.chaninfo.rate, &pktBuf[idx], 1);
    idx += 1;

    // priority
    memcpy(&wsmReq.txpriority, &pktBuf[idx], 1);
    idx += 1;

    // packet contents
    const int conLen = pktLen-6;
    assert(conLen > 0);
    assert(conLen < FOURK);
    wsmReq.data.length = conLen;
    memcpy(&wsmReq.data.contents, &pktBuf[idx], conLen);
    idx += conLen;
    assert(idx == pktLen);
    
    // destination mac is bytes 0-5 of the packet contents
    memcpy(&wsmReq.macaddr, &wsmReq.data.contents[0], IEEE80211_ADDR_LEN);

    // source mac is bytes 6-11 of the packet contents
    memcpy(&wsmReq.srcmacaddr, &wsmReq.data.contents[IEEE80211_ADDR_LEN], \
        IEEE80211_ADDR_LEN);
    

#ifdef DEBUG
if (conLen > 400){
    std::cout << "Sending wsm " << std::dec << nTx++ << ", " << std::dec << conLen << " bytes, from mac: ";

    for (int i=0; i < IEEE80211_ADDR_LEN; i++){
        const uint8_t byte = wsmReq.srcmacaddr[i];
        std::cout << std::hex << std::setfill('0') << \
        std::setw(2) << static_cast<unsigned>(byte);
        if (i < (IEEE80211_ADDR_LEN-1)){
            std::cout << ":";
        }
    }

    std::cout << ", to mac: ";
    for (int i=0; i < IEEE80211_ADDR_LEN; i++){
        const uint8_t byte = wsmReq.macaddr[i];
        std::cout << std::hex << std::setfill('0') << \
        std::setw(2) << static_cast<unsigned>(byte);
        if (i < (IEEE80211_ADDR_LEN-1)){
            std::cout << ":";
        }
    }
    std::cout << std::endl;
}
#endif

    // the packet is now fully assembled and ready to be sent

    // send WSM
    if (txWSMPacket(_pid, &wsmReq) < 0){
        std::cerr << "handleClient(), txWSMPacket(): " << strerror(errno) << \
            std::endl;
    }
}


/**
 * Process requests from a client for as long as he remains connected.
 */
void handleClient(){

    // reception helpers
    uint8_t rcvBuf[FOURK];
    uint8_t pktBuf[FOURK];
    uint16_t pktLen=0, pktBufIdx=0;

    int nrcvd;
    while (true){

        if((nrcvd = recv(_cliSockFd, rcvBuf, FOURK, 0 /*flags*/)) > 0){
        
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

                } else { // in the middle of a packet

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

                    if (nmissing == ncpy) { // got complete packet, send it out!
                        assert(pktBufIdx == pktLen);

                        // parse and send it out the air interface
                        parseAndSendWSM(pktBuf, pktLen);

                        // reset the packet buffer state
                        pktLen = 0;
                        pktBufIdx = 0;
                    }
                }
            }
        } else if (nrcvd == 0){ // means client disconnected
            break; // exit method and 
        } else { // means nrcvd < 0
            std::cerr << "handleClient(), recv(): " << strerror(errno) << \
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

    // wave role
	try {
		const uint8_t roleCode = (uint8_t) \
            std::atoi(conf.Value("wave", "role").c_str());

        if (roleCode == 0)
            _waveRole = WAVE_PROVIDER;
        else if (roleCode == 1)
            _waveRole = WAVE_USER;
        else
            throw "role must be 0 (provider) or 1 (user)";

	} catch (char const *str){
		std::cerr << "Config error: section=wave, value=role (" << str \
            << "), using default value " << unsigned(_waveRole) << "." << \
            std::endl;
	}


    // channel
    uint8_t pChannel = 172;
	try {
		const uint8_t confChannel = (uint8_t) \
            std::atoi(conf.Value("wave", "channel").c_str());

        if (not (confChannel >= 172 and confChannel <= 182 and \
            (confChannel % 2) == 0)){
            throw "channel number must be in (172,174,176,178,180,182)";
        }

        pChannel = confChannel;

	} catch (char const *str){
		std::cerr << "Config error: section=wave, value=channel (" << str \
            << "), using default value " << unsigned(pChannel) << "." << \
            std::endl;
	}

    // psid
	try{
		_pPsid = (uint32_t) std::atol(conf.Value("wave", "psid").c_str());

	} catch (char const *str) {
		std::cerr << "Config error: section=wave, value=psid (" << str \
            << "), using default value " << _pPsid << "." << std::endl;
	}
    
    // priority
	uint8_t pPriority = 7;
	try{
		const uint8_t confPriority = \
            (uint8_t) std::atoi(conf.Value("wave", "priority").c_str());

        if (confPriority > 7){
            throw "priority must be in [0,7]";
        }
        
        pPriority = confPriority;

	} catch (char const *str) {
		std::cerr << "Config error: section=wave, value=priority (" << str \
            << "), using default value " << unsigned(pPriority) << "." << \
            std::endl;
	}

    // port
    std::string pPort = "45622";
	try{
		const unsigned port = (unsigned int) \
            std::atol(conf.Value("server", "port").c_str());
            
        if (port < 1024 or port > 65535){
            throw "port must be in [1024,65535]";
        }

        pPort = conf.Value("server", "port");

	} catch (char const *str) {
		std::cerr << "Config error: section=server, value=port (" << str \
            << "), using default value " << pPort << "." << std::endl;
	}

    // 2. WAVE SET UP

    // build PST entry
    memset(&_wmeReq, 0, sizeof(WMEApplicationRequest)); // clean slate
    _wmeReq.channel = pChannel;
    _wmeReq.psid = _pPsid;
    _wmeReq.priority = pPriority;

    _pid = getpid();

    try {

        if (invokeWAVEDevice(WAVEDEVICE_LOCAL, 255 /*blockflag*/) < 0){

            std::ostringstream ss;
            ss << "invokeWAVEDevice(): " << std::strerror(errno);
            throw ss.str();
        }

        if (_waveRole == WAVE_PROVIDER){ // provider role

            _wmeReq.repeatrate = 50; // #msgs p/ 5 secs
            _wmeReq.channelaccess = CHACCESS_CONTINUOUS;
            _wmeReq.serviceport = 8888;

            if (registerProvider(_pid, &_wmeReq) < 0){
                std::ostringstream ss;
                ss << "registerProvider(): " << std::strerror(errno);
                throw ss.str();
            }

        }
        else { // user role

            assert(_waveRole == WAVE_USER); // no other option

            // user-specific fields
            _wmeReq.userreqtype = USER_REQ_SCH_ACCESS_AUTO_UNCONDITIONAL;
            _wmeReq.schaccess = 1; // immediate access
            _wmeReq.schextaccess = 1; // extended access

            if (registerUser(_pid, &_wmeReq) < 0){
                std::ostringstream ss;
                ss << "registerUser(): " << std::strerror(errno);
                throw ss.str();    
            }
        }

        _waveReg = true; // provider now registered

    } catch (const std::string &str){

        std::cerr << "Error setting up WAVE: " << str << ". Aborting." << \
            std::endl;
        die();
    }

#ifdef DEBUG
    std::cout << "WAVE setup completed" << std::endl;
#endif

    // 3. LAUNCH RECEIVER THREAD
    if (pthread_create(&_rcvrTid, NULL, receiver_thread, NULL /*arg*/)){
        std::cerr << "main(), pthread_create(&_rcvrTid): " << \
            strerror(errno) << std::endl;
        die();
    }

    // 4. SIGNAL HANDLING SET UP
    // set up termination signal handler
    signal(SIGINT, sigHandler);
	signal(SIGTERM, sigHandler);
	signal(SIGHUP, sigHandler);

    // 5. BRING SERVER UP

    // get the address info
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, pPort.c_str(), &hints, &res) != 0) {
        std::cerr << "main(), getaddrinfo(): " << strerror(errno) << std::endl;
        die();
    }

    // create the (server) socket
    if ((_servSockFd = socket(res->ai_family, res->ai_socktype, \
        res->ai_protocol)) == -1){
        std::cerr << "main(), socket(): " << strerror(errno) << std::endl;
        die();
    }

    // enable the socket to reuse the address
    const int reuseaddr = 1; // true
    if (setsockopt(_servSockFd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, \
        sizeof(int)) == -1) {
        std::cerr << "main(), setsockopt(): " << strerror(errno) << std::endl;
        die();
    }

    // bind to the address
    if (bind(_servSockFd, res->ai_addr, res->ai_addrlen) == -1) {
        std::cerr << "main(), bind(): " << strerror(errno) << std::endl;
        die();
    }

    // listen on the socket
    if (listen(_servSockFd, 0 /*backlog*/) == -1) {
        std::cerr << "main(), listen(): " << strerror(errno) << std::endl;
        die();
    }

    freeaddrinfo(res);

#ifdef DEBUG
    std::cout << "Server up and listening" << std::endl;
#endif

    // 6. MAIN LISTEN LOOP
    
    while (true) {
    
        size_t sockaddrLen = sizeof(struct sockaddr_in);
        struct sockaddr_in clientAddr;

        if ((_cliSockFd = accept(_servSockFd, \
            (struct sockaddr*)&clientAddr, &sockaddrLen)) == -1) { // failure
        
            std::cerr << "main(), accept(): " << strerror(errno) << std::endl;
            
        } else{ // success!

#ifdef DEBUG
            std::cout << "Got a client from " << \
                inet_ntoa(clientAddr.sin_addr) << " on port " << \
                htons(clientAddr.sin_port) << std::endl;
#endif
            handleClient();
            _cliSockFd = 0;
           
#ifdef DEBUG
            std::cout << "Client disconnected" << std::endl;
#endif
        }
    }
    
    die();

    return 0;
}

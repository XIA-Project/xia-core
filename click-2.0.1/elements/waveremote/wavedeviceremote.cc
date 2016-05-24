/**
 * wavedeviceremote.{cc,hh} -- wave device remote interface
 * Rui Meireles {rui@cmu.edu}
 *
 * Copyright 2016 Carnegie Mellon University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <click/config.h> /* CLICK_DECLS */
#include <click/glue.hh> /* click_chatter(), random() */
#include <click/args.hh> /* Args() */
#include <click/standard/scheduleinfo.hh> /* ScheduleInfo */
#include <click/packet.hh> /* Packet::default_headroom */
#include <clicknet/ether.h> /* click_ether */
#include <click/vector.hh> /* Vector */
#include <click/string.hh> /* String */

#include <stdint.h> /* uint*_t */
#include <cassert> /* assert() */
#include <unistd.h> /* pipe() */
#include <pthread.h> /* pthread_create, pthread_cancel, etc */
#include <sys/types.h> /* connect() */
#include <sys/socket.h> /* connect(), send(), recv() */
#include <arpa/inet.h> /* htons(), htonl(), etc... */
#include <netdb.h> /* getaddrinfo() */

#include <iomanip> // std::hex, etc
#include <cstring> /* memcpy() */

#include "wavedeviceremote.hh"

#define WSMP_MTU 1024 // in theory up to 4096 bytes given 12 bit length field
                      // but arada only supports up to 1400 bytes at the moment

CLICK_DECLS


WaveDeviceRemote::WaveDeviceRemote() : _task(this),
                                       _wq(0),
                                       _rcvrTid(0){
    _pipeFd[0] = 0;
    _pipeFd[1] = 0;
}


WaveDeviceRemote::~WaveDeviceRemote(){
}


int WaveDeviceRemote::configure(Vector<String> &conf, ErrorHandler *errh){

    String hostname;
    int port = 45622;
	int txPower = 23;
    double dataRate = 3;
    uint32_t  channel = 172;
    int userPrio = 7;
    int bufLen = 4096;
    int headroom = Packet::default_headroom;
    int setTstamp = false;

	int res;
    if ((res = Args(conf, this, errh)
		 .read_m("HOSTNAME", hostname)
         .read("PORT", port)
		 .read("CHANNEL", channel)
		 .read("TXPOWER", txPower)
		 .read("DATARATE", dataRate)
		 .read("USERPRIORITY", userPrio)
         .read("SETTSTAMP", setTstamp)
         .read("BUFLEN", bufLen)
         .read("HEADROOM", headroom)
		 .complete()) < 0){
		return res;
	}

    int retval = 0;

    // check and set configuration parameters

    // hostname
    _hostname = hostname; // no validation at this point

    // port
    if (port < 1024 or port > 65535){

        errh->error("%s, configure(): invalid channel %d, please specify one of \
{172, 174, 176, 178, 180, 182, 184}.", declaration().c_str(), channel);
        retval = -1;
    } else{
        _port = port;
    }

    // channel
    if (not (channel % 2 == 0 and channel >= 172 and channel <= 184)){
        errh->error("%s, configure(): invalid channel %d, please specify one of \
{172, 174, 176, 178, 180, 182, 184}.", declaration().c_str(), channel);
        retval = -1;
    } else
        _channel = channel;

    // txpower
    if (txPower < 1 or txPower > 23){
        errh->error("%s, configure(): invalid txPower %d, please specify value \
in [1,23].", declaration().c_str(), txPower);
        retval = -1;
    } else
        _txPower = txPower;

    // data rate
    double supDataRates[] = {3, 3, 4.5, 6, 9, 12, 18, 24, 27};
    int len = sizeof(supDataRates)/sizeof(double);
    bool dataRateOk = false;

    for (int i=0; i < len; i++)
        if (dataRate == supDataRates[i]){
            _dataRateIndex = i;
            dataRateOk = true;
            break;
        }

    if (not dataRateOk){
        errh->error("%s, configure(): invalid data rate %f, please specify one \
of {3, 4.5, 6, 9, 12, 18, 24, 27}.", declaration().c_str(), dataRate);
        retval = -1;
    }
    
    // user priority
    if (userPrio < 0 or userPrio > 7){
        errh->error("%s, configure(): invalid user priority %d, please specify \
a priority in [0,7].", declaration().c_str(), userPrio);
        retval = -1;
    } else
        _userPrio = userPrio;

    // set timestamp
    _setTstamp = setTstamp;

    // buffer length
    if (bufLen <= 0){
        errh->error("%s, configure(): invalid bufLen %d, please specify a \
value larger than zero.", declaration().c_str(), bufLen);
        retval = -1;
    } else
        _bufLen = bufLen;
    
    // head room
    if (headroom < 0){
        errh->error("%s, configure(): invalid headroom %d, please specify a \
non-negative value.", declaration().c_str(), headroom);
        retval = -1;
    } else
        _headroom = headroom;

	return retval;
}


/**
 * Register WAVE device, register upstream notification (if there are inputs) 
 * and launch receiver thread (if there are outputs).
 */
int WaveDeviceRemote::initialize(ErrorHandler *errh){

    struct addrinfo hints, *servinfo = NULL, *p;
    
    // prepare the hints
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // don't care if IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;


    // attempt to connect to remote WAVE device
    bool connectok = false;

    // try to connect to the server
    for(int i = 0; i < 2; i++){

        // loop through all the results and connect to the first we can
        for(p = servinfo; p != NULL; p = p->ai_next) {

            if ((_remoteSockFd = socket(p->ai_family, p->ai_socktype, 
                p->ai_protocol)) == -1) {
                
                errh->error("%s, initialize(): socket() error trying address, \
strerror=%s", declaration().c_str(), strerror(errno));
                continue;
            }

            if (connect(_remoteSockFd, p->ai_addr, p->ai_addrlen) == -1) {

                errh->error("%s, initialize(): connect() error trying address, \
strerror=%s", declaration().c_str(), strerror(errno));

                close(_remoteSockFd);
                continue;
            }

            // if we get this far, we must have connected successfully
            connectok = true;
            break;
        }
            
        // if already connected or on the last iteration, no need to go on
        if (connectok or i == 1)
            break;

        if (servinfo)
            freeaddrinfo(servinfo);

        String port(_port);
        if (getaddrinfo(_hostname.c_str(), port.c_str(), &hints, &servinfo) \
            != 0) {

                errh->error("%s, initialize(): getaddrinfo() strerror=%s", \
                    declaration().c_str(), strerror(errno));
            break;
        }
    }
    
    if (!connectok){
        return errh->error("%s, initialized failed to connect, check provided \
hostname and port (and make sure the server is up).", declaration().c_str());
    }

    // allocate memory for pipeBuf
    _pipeBuf = new uint8_t[_bufLen];

    try {

        // initialize pipe mutex
        if (pthread_mutex_init(&_pipeMutex, NULL))
            throw "pthread_mutex_init(&_pipeMutex)";

        // packets coming in?
        if (ninputs() && input_is_pull(0)){
            ScheduleInfo::join_scheduler(this, &_task, errh);
            _signal = Notifier::upstream_empty_signal(this, 0, &_task);
        }

        if (noutputs()){
            // create the pipe
            if (pipe(_pipeFd) < 0)
                throw "pipe(_pipeFd)";
     
            add_select(_pipeFd[0], SELECT_READ); // wake on data available
           
            if (pthread_create(&_rcvrTid, NULL, receiver_thread, (void *)this))
                throw "pthread_create(&_rcvrTid)";
        }
    } catch (const char *str){

        int e = errno; // preserve errno

        assert(_isConnected);
        
        if (close(_remoteSockFd) == -1){
            errh->error("%s, initialize(): close(_remoteSockFd), strerror=%s", \
                declaration().c_str(), strerror(errno));
        } else{
            _isConnected = false;
        }

        remove_select(_pipeFd[0], SELECT_READ | SELECT_WRITE);

        return errh->error("%s, initialize(): %s, strerror=%s", \
            declaration().c_str(), str, strerror(e));
    }

    return 0;
}


void WaveDeviceRemote::add_handlers(){
    add_task_handlers(&_task);
}

/**
 * Release all allocated resources in preparation for shutdown.
 */
void WaveDeviceRemote::cleanup(CleanupStage){

    // clean read and write buffers
    if (_wq){
        _wq->kill();
    }

    remove_select(_pipeFd[0], SELECT_READ | SELECT_WRITE);

    ErrorHandler *errh = ErrorHandler::default_handler();

    if (noutputs()){ // if receiving packets

        // cancel the receiver thread (not joined because infinite loop)
        if(_rcvrTid != 0 and pthread_cancel(_rcvrTid))
            errh->error("%s, cleanup(): pthread_cancel(_rcvrTid), strerror=%s", \
                declaration().c_str(), strerror(errno));

        // close reading side of pipe
        if (_pipeFd[0] != 0 and close(_pipeFd[0]) < 0)
            errh->error("%s, cleanup(): close(_pipeFd[0]), strerror=%s", \
                declaration().c_str(), strerror(errno));


        // close writing side of pipe
        if (_pipeFd[1] != 0 and close(_pipeFd[1]) < 0)
            errh->error("%s, cleanup(): close(_pipeFd[1]), strerror=%s", \
                declaration().c_str(), strerror(errno));

    }

    // destroy pipe mutex
    if (pthread_mutex_destroy(&_pipeMutex))
        errh->error("%s, cleanup(): pthread_mutex_destroy(&_pipeMutex), \
strerror=%s", declaration().c_str(), strerror(errno));

    // free pipe buffer
    delete[] _pipeBuf;
    
    // disconnect from WAVE device
    if (_isConnected){
        if (close(_remoteSockFd) == -1){
            errh->error("%s, cleanup(): close(_remoteSockFd), strerror=%s", \
                declaration().c_str(), strerror(errno));
        }
    }
    _isConnected = false;    
}


/**
 * Process an input packet (push mode).
 */
void WaveDeviceRemote::push(int, Packet *p) {

    assert(_isConnected); // because initialize runs before

    ErrorHandler *errh = ErrorHandler::default_handler();

    int err;

    // write
    do {
        err = write_packet(p, errh);
    } while (err < 0 && (errno == ENOBUFS || errno == EAGAIN));

    if (err < 0){
        errh->error("%s, push(): write_packet(), strerror=%s, dropping packet",\
            declaration().c_str(), strerror(err));
        p->kill();
    }
}


/**
 * Process an input packet (pull mode).
 */
bool WaveDeviceRemote::run_task(Task*){

    assert(_isConnected); // because initialize runs before

    bool retval = false;
    assert(ninputs() and input_is_pull(0));
    ErrorHandler *errh = ErrorHandler::default_handler();
    
    Packet *p;
    if (_wq) {
        p = _wq;
        _wq = 0;
    } else
        p = input(0).pull();

    if (p) {
    
        // mind the MTU
        if (p->length() > WSMP_MTU){
            errh->error("%s, run_task(): packet larger than WSM MTU (%d vs %d \
bytes), dropping", declaration().c_str(), p->length(), WSMP_MTU);
            p->kill();

        } else{

            int err = write_packet(p, errh);

            // try again later w/ the same packet
            if (err < 0 && (errno == ENOBUFS || errno == EAGAIN)){
                _wq = p;
            } else if (err < 0) {
                errh->error("%s, run_task(), write_packet(), strerror=%s, \
dropping", declaration().c_str(), strerror(err));
                p->kill();
            }
            else
                retval = true;
        }
    }

    // if notifier signal still active, upstream won't reschedule the task,
    // we have to do it ourselves
    if (_signal){
        _task.fast_reschedule();
    }

    return retval;
}


/**
 * Send a packet to the WAVE device.
 */
int WaveDeviceRemote::write_packet(Packet *p, ErrorHandler *errh){

    assert(_isConnected); // because initialize runs before

    // calculate packet length
    // 2 (length) + 1 (channel) + 1 (txpower) + 1 (data rate index) + 
    // 1 (user priority) + content length = 6 + content length
    const int pktLen = 6 + p->length();

    // serialize everything
    uint8_t pktBuf[_bufLen];
    
    int idx=0;
    
    // packet length
    const uint16_t pktLenNet = htons((uint16_t) pktLen);
    memcpy(&pktBuf[idx], &pktLenNet, 2);
    idx += 2;
    
    // channel
    memcpy(&pktBuf[idx], &_channel, 1);
    idx += 1;
    
    // txpower
    memcpy(&pktBuf[idx], &_txPower, 1);
    idx += 1;
    
    // data rate index
    memcpy(&pktBuf[idx], &_dataRateIndex, 1);
    idx += 1;
    
    // user priority
    memcpy(&pktBuf[idx], &_userPrio, 1);
    idx += 1;

    // packet contents
    memcpy(&pktBuf[idx], p->data(), p->length());
    idx += p->length();

    assert(pktLen == idx); // pktlen does not include the length field

    bool done = false;
    while (not done){

#ifdef DEBUG        
            errh->debug("%s, sending server %d bytes (%d byte payload)", \
                declaration().c_str(), pktLen, p->length());
#endif

        errno = 0;
        if (send(_remoteSockFd, pktBuf, pktLen, 0 /*flags*/) < 0){ // error

            // out of memory or would block
            if (errno == ENOBUFS || errno == EAGAIN)
                return -1;
            
            // interrupted by signal, try again immediately
            else if (errno == EINTR)
                continue;
            
            // connection probably terminated or other fatal error
            else {
                errh->error("%s, write_packet(): strerror=%s", \
                    declaration().c_str(), strerror(errno));
                break;
            }
        } else
            
#ifdef DEBUG        
            errh->debug("%s, sent WSM, %d byte payload", \
                declaration().c_str(), p->length());
#endif
            done = true;
        }

    p->kill();
    
    return 0;
}


/**
 * A thread that blocks waiting for incoming packets.
 * It was the most elegant solution I found for the problem.
 */
void* WaveDeviceRemote::receiver_thread(void *arg){

    // this is an infinite-loop type thread
    // join() won't work so enable cancellation
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    ErrorHandler *errh = ErrorHandler::default_handler();

    // get parameters from class instance
    WaveDeviceRemote* wdrInst = (WaveDeviceRemote*) arg;
    assert(wdrInst->isConnected());

    pthread_mutex_t* pipeMutex = wdrInst->get_pipeMutex();
    const int pipeFd = wdrInst->get_pipefd(true);
    const int bufLen = wdrInst->get_bufLen();
    const int remoteSockFd = wdrInst->get_remoteSockFd();

    uint8_t rcvBuf[bufLen];
    uint8_t pktBuf[bufLen];
    uint16_t pktLen=0, pktBufIdx=0;

    // infinite read loop
    // the packet format is 2 bytes for the packet length, and then the packet
    // itself
    int nrcvd;
    while (true){

        if ((nrcvd = recv(remoteSockFd, rcvBuf, bufLen, 0 /*flags*/)) > 0){

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

#ifdef DEBUG
                        errh->debug("%s, extracted a rcvd packet w/ %d bytes",\
                            wdrInst->declaration().c_str(), pktLen);
#endif

                        if (pthread_mutex_lock(pipeMutex))
                            errh->error("%s, receiver_thread(): \
pthread_mutex_lock(pipeMutex), strerror=%s", 
                                wdrInst->declaration().c_str(), \
                                strerror(errno));

                        // write the received data on the pipe
                        const int dataLen = pktLen-2;
                        if (write(pipeFd, &pktBuf[2], dataLen) != dataLen)
                            errh->error("%s, receiver_thread(): write(pipeFd), \
strerror=%s", wdrInst->declaration().c_str(), strerror(errno));

                        // reset the packet buffer state
                        pktLen = 0;
                        pktBufIdx = 0;
                    }
                }
            }

        } else if (errno != EAGAIN){
            errh->error("recv: %s", strerror(errno));
        }
    }

    return 0;
}


/**
 * This method is used to wake up the router when a new packet is received
 * by the receiver thread.
 */
void WaveDeviceRemote::selected(int fd, int /*mask*/){

    // basically read from the pipe that the receiver thread writes
    // to whenever it receives a packet.
    assert (fd == _pipeFd[0]);  // nothing else it could be

    const ssize_t nbytesRead = read(_pipeFd[0], _pipeBuf, _bufLen);

    if (pthread_mutex_unlock(&_pipeMutex)){
        ErrorHandler *errh = ErrorHandler::default_handler();
        errh->error("%{element}: pipeFd unlock: %s", this, strerror(errno));
    }

    if (nbytesRead < 0){
        ErrorHandler *errh = ErrorHandler::default_handler();
        errh->error("%{element}: pipeFd read: %s", this, strerror(errno));
    }

    // now transform the bytes that were read into a packet
    WritablePacket* rq = Packet::make(_headroom, /* headroom */
                                      _pipeBuf, /* data */
                                      nbytesRead, /* length */
                                      0 /* tailroom */);

    if (rq){ // non zero means success, I believe

        // set the ethernet header pointer
        rq->set_mac_header(rq->data());

        if (_setTstamp) // set timestamp annotation if that is the case
            rq->timestamp_anno().assign_now();

        assert(rq->end_data()-rq->data() == nbytesRead); // values must agree

        checked_output_push(0, rq);
        rq = 0;
    }
}


// getters useful for passing arguments to receiver thread
bool WaveDeviceRemote::isConnected() const{
    return _isConnected;
}


int WaveDeviceRemote::get_bufLen() const{
    return _bufLen;
}


int WaveDeviceRemote::get_remoteSockFd() const{
    return _remoteSockFd;
}


int WaveDeviceRemote::get_pipefd(const bool write) const{

    if (write)
        return _pipeFd[1]; // write on [1]
    else
        return _pipeFd[0]; // read on [0]
}


pthread_mutex_t* WaveDeviceRemote::get_pipeMutex(){
    return &_pipeMutex;
}


CLICK_ENDDECLS
EXPORT_ELEMENT(WaveDeviceRemote)
ELEMENT_LIBS(-lpthread)

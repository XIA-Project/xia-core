/**
 * wavedevicelocal.{cc,hh} -- wave device interface
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

#include <sstream>
#include <iomanip>

extern "C" {
    #include <arada/wave.h> /* wave lib: txWSMPacket(), etc... */
}


#include "wavedevicelocal.hh"

#define WSMP_MTU 1024 // in theory up to 4096 bytes given 12 bit length field
                      // but arada only supports up to 1400 bytes at the moment


CLICK_DECLS

// static members initialization
// these guys are not declared in the header because then we'd need to include
// wave.h in the header file and since, a linking error re. struct variable
// named GETAvailableServiceInfo will arise. The include guards on wave.h
// only work for code in the same compilation unit and click compiles 
// wavedevicelocal.cc and elements.cc (which includes all the element headers)
// separately. It's just bad practice to declare variables in header files
// but what can we do? We have to work with wave.h.
static WMEApplicationRequest _wmeReq; // don't know why static but must
static WSMRequest _wsmReq; // don't know why static but must


enum {WAVE_PROVIDER, WAVE_USER}; // WAVE roles


WaveDeviceLocal::WaveDeviceLocal() : _task(this),
                                     _wq(0),
                                     _rcvrTid(0),
                                     _pid(0),
                                     _isWaveRegistered(false) {

    _pipeFd[0] = 0;
    _pipeFd[1] = 0;
}


WaveDeviceLocal::~WaveDeviceLocal(){
}


int WaveDeviceLocal::configure(Vector<String> &conf, ErrorHandler *errh){

    String roleStr;
    uint32_t psid = 5;
	int txPower = 23;
    double dataRate = 6;
    uint32_t channel = 172;
    int userPrio = 1;
    int expiryTime = 0;
    int bufLen = 2048;
    int headroom = Packet::default_headroom;
    int setTstamp = false;

	int res;
    if ((res = Args(conf, this, errh)
		 .read_m("ROLE", roleStr)
         .read("PSID", psid)
		 .read("CHANNEL", channel)
		 .read("TXPOWER", txPower)
		 .read("DATARATE", dataRate)
		 .read("USERPRIORITY", userPrio)
		 .read("EXPIRYTIME", expiryTime)
         .read("SETTSTAMP", setTstamp)
         .read("BUFLEN", bufLen)
         .read("HEADROOM", headroom)
		 .complete()) < 0){
		return res;
	}

    int retval = 0;

    // WAVE config

    // role
    roleStr = roleStr.lower();
    _role = WAVE_PROVIDER;
    
    if (roleStr == "user")
        _role = WAVE_USER;
    else if (roleStr != "provider"){
        errh->error("%s, configure(): invalid WAVE role %s, please specify \
one of {user, provider}.", declaration().c_str(), roleStr.c_str());
        retval = -1;
    }

    // psid
    _psid = psid; // no validation at this point

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

    // expiry time
    if (expiryTime < 0){
        errh->error("%s, configure(): invalid expiry time %d, please specify a \
non-negative value.", declaration().c_str(), expiryTime);
        retval = -1;
    } else
        _expiryTime = expiryTime;
    
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
int WaveDeviceLocal::initialize(ErrorHandler *errh){

    _pid = getpid(); // save the process ID, required by the WAVE driver

    if (invokeWAVEDevice(WAVEDEVICE_LOCAL, 255 /*blockflag*/) < 0)
        return errh->error("%s, initialize(): invokeWAVEDevice(), strerror=%s",\
            declaration().c_str(), strerror(errno));

    // register provider or user depending
    // start with a clean slate
    memset(&_wmeReq, 0, sizeof(WMEApplicationRequest));

    // fields common to both provider and user
    _wmeReq.channel = _channel;
    _wmeReq.psid = _psid;
    
    _wmeReq.channelaccess = CHACCESS_CONTINUOUS;

    if (_role == WAVE_PROVIDER){ // provider role

        // provider-specific fields
        _wmeReq.priority = _userPrio;
        _wmeReq.repeatrate = 50; // #msgs p/ 5 secs
        _wmeReq.channelaccess = CHACCESS_CONTINUOUS;
        _wmeReq.serviceport = 8888;

        // register
        if (registerProvider(_pid, &_wmeReq) < 0)
            return errh->error("%s, initialize(): registerProvider(), \
strerror=%s", declaration().c_str(), strerror(errno));

    } else { // user role
    
        assert(_role == WAVE_USER); // no other option

        // user-specific fields
        _wmeReq.userreqtype = USER_REQ_SCH_ACCESS_AUTO_UNCONDITIONAL;
        _wmeReq.schaccess = 1; // immediate access
        _wmeReq.schextaccess = 1; // extended access

        if (registerUser(_pid, &_wmeReq) < 0)
            return errh->error("%s, initialize(): registerUser(), strerror=%s",\
                declaration().c_str(), strerror(errno));
    }

    _isWaveRegistered = true; // successful registration

    // set up wsm request for WSM transmission
    _wsmReq.version = 1;
    _wsmReq.security = 0;
    _wsmReq.chaninfo.channel = _channel;
    _wsmReq.chaninfo.rate = _dataRateIndex;
    _wsmReq.chaninfo.txpower = _txPower;
    _wsmReq.psid = _psid;
    _wsmReq.txpriority = _userPrio;

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

        // unregister WAVE
        if (_role == WAVE_PROVIDER)
            removeProvider(_pid, &_wmeReq);
        else{
            removeUser(_pid, &_wmeReq);
            assert(_role == WAVE_USER); // no other option
        }
    
        _isWaveRegistered = false;

        remove_select(_pipeFd[0], SELECT_READ | SELECT_WRITE);

        return errh->error("%s, initialize(): %s, strerror=%s", \
            declaration().c_str(), str, strerror(e));
    }

    return 0;
}


void WaveDeviceLocal::add_handlers(){
    add_task_handlers(&_task);
}

/**
 * Release all allocated resources in preparation for shutdown.
 */
void WaveDeviceLocal::cleanup(CleanupStage){

    // unregister WAVE provider/user
    if (_isWaveRegistered){

        if (_role == WAVE_PROVIDER)
            removeProvider(_pid, &_wmeReq);
        else{
            removeUser(_pid, &_wmeReq);
            assert(_role == WAVE_USER); // no other option
        }
    }
    _isWaveRegistered = false;

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
}


/**
 * Process an input packet (push mode).
 */
void WaveDeviceLocal::push(int, Packet *p) {

    assert(_isWaveRegistered); // because initialize runs before

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
bool WaveDeviceLocal::run_task(Task*){

    assert(_isWaveRegistered); // because initialize runs before

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
 * Write a packet to the WAVE device.
 */
int WaveDeviceLocal::write_packet(Packet *p, ErrorHandler *errh){

    assert(_isWaveRegistered); // because initialize runs before

    // set mac addresses, this is the part that assumes the received
    // packet is an ethernet frame
    const click_ether *ethHeader = p->ether_header();
    // source mac
    memcpy(&_wsmReq.srcmacaddr, &(ethHeader->ether_shost), IEEE80211_ADDR_LEN);

    // destination mac
    memcpy(&_wsmReq.macaddr, &(ethHeader->ether_dhost), IEEE80211_ADDR_LEN);

    // set contents
    memcpy(&_wsmReq.data.contents, p->data(), p->length());
    _wsmReq.data.length = p->length();

    bool done = false;
    while (not done){

        errno = 0;
        if (txWSMPacket(_pid, &_wsmReq) < 0){ // error

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
void* WaveDeviceLocal::receiver_thread(void *arg){

    // this is an infinite-loop type thread
    // join() won't work so enable cancellation
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    ErrorHandler *errh = ErrorHandler::default_handler();

    // get parameters from class instance
    WaveDeviceLocal* waveDeviceInst = (WaveDeviceLocal*) arg;
    const pid_t pid = waveDeviceInst->get_pid();
    pthread_mutex_t* pipeMutex = waveDeviceInst->get_pipeMutex();
    const int pipeFd = waveDeviceInst->get_pipefd(true);
    const int bufLen = waveDeviceInst->get_bufLen();

    // prepare wsm indication data structure
    WSMIndication rxPkt;

    // infinite read loop
    int len;
    while (true){
    
        memset(&rxPkt, 0, sizeof(WSMIndication)); // playing it safe

        if ((len = rxWSMPacket(pid, &rxPkt)) > 0){ // got a packet, yay
        
#ifdef DEBUG        
            errh->debug("%s, received WSM, %d byte payload", \
                waveDeviceInst->declaration().c_str(), rxPkt.data.length);
#endif

            if (rxPkt.data.length <= 0)
                continue; // nothing to do if empty payload

            assert(rxPkt.data.length <= bufLen);

            if (pthread_mutex_lock(pipeMutex))
                errh->error("%s, receiver_thread(): \
pthread_mutex_lock(pipeMutex), strerror=%s", 
                    waveDeviceInst->declaration().c_str(), strerror(errno));

            // write the received data on the pipe
            if (write(pipeFd, rxPkt.data.contents, rxPkt.data.length) != \
                rxPkt.data.length)
                errh->error("%s, receiver_thread(): write(pipeFd), \
strerror=%s", waveDeviceInst->declaration().c_str(), strerror(errno));

        } else if (errno != EAGAIN)
            errh->error("recv: %s", strerror(errno));
    }

    return 0;
}

/**
 * This method is used to wake up the router when a new packet is received
 * by the receiver thread.
 */
void WaveDeviceLocal::selected(int fd, int /*mask*/){

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
pid_t WaveDeviceLocal::get_pid() const{
    return _pid;
}


int WaveDeviceLocal::get_bufLen() const{
    return _bufLen;
}


int WaveDeviceLocal::get_pipefd(const bool write) const{

    if (write)
        return _pipeFd[1]; // write on [1]
    else
        return _pipeFd[0]; // read on [0]
}


pthread_mutex_t* WaveDeviceLocal::get_pipeMutex(){
    return &_pipeMutex;
}


CLICK_ENDDECLS
EXPORT_ELEMENT(WaveDeviceLocal)
ELEMENT_LIBS(-lpthread -lwave)

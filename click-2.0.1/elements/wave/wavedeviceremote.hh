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

#ifndef CLICK_WAVEDEVICEREMOTE_HH
#define CLICK_WAVEDEVICEREMOTE_HH

#include <click/element.hh>
#include <click/vector.hh> /* Vector */
#include <click/string.hh> /* String */
#include <click/error.hh> /* ErrorHandler */
#include <click/notifier.hh> /* SignalNotifier */
#include <click/task.hh> /* Task */

#include <stdint.h> /* uint*_t */
#include <pthread.h> /* pthread_mutex_t */



CLICK_DECLS

/*
 * =c
 * WaveDeviceRemote()
 * =s local
 * Provides a device interface to a remote WAVE device (WSMP protocol portion),
 * that we connect to through TCP.
 *
 * =d
 *
 * If an input port is specified (either push or pull), incoming packets get
 * passed to the WAVE device. Packets arriving on the input port are expected
 * to begin with an ethernet header, which is used to set the source and
 * destination MAC addresses on the WSM request.
 *
 * If an output port is specified (always of the push variety), packets that 
 * coming from the WAVE device are pushed through it.
 *
 * =over 8
 *
 * =item HOSTNAME
 *
 * Remote hostname (string). This is a mandatory argument.
 *
 * =item PORT
 *
 * Remote port the WAVE device server is listening on. Default is 34569.
 *
 * =item PSID
 *
 * The provider service ID, a 32 bit unsigned integer. Default is 32.
 * 
 * =item CHANNEL
 *
 * The communication channel number for WAVE communication (continuous access).
 * One of 172, 174, 176, 178, 180, 182, 184  in each slot. Default is 178 (CCH).
 *
 * =item TXPOWER
 *
 * The power, in dBm, at which WAVE packets are transmitted.
 * Between 1 and 23. Default is 23.
 *
 * =item DATARATE
 *
 * The data rate at which packets are sent. 
 * One of 3, 4.5, 6, 9, 12, 18, 24, 27.
 * Default is 27.
 *
 * =item USERPRIORITY
 *
 * WAVE user priority. Between 0 and 7. 
 * Higher value means higher priority. The default is 1.
 *
 * =item BUFLEN
 *
 * Send/receive buffer length in bytes. Default is 4096.
 *
 * =item HEADROOM
 *
 * Headroom for incoming packets to create for incoming packets. Default is Packet::default_headroom.
 *
 * =back
 *
 * =e
 * This is a typical input processing sequence:
 *
 * ... -> WaveDeviceRemote() -> ...
 *
 * =a WaveDevice
 */

class WaveDeviceRemote : public Element { public:

    WaveDeviceRemote();
    ~WaveDeviceRemote();

    const char *class_name() const	{ return "WaveDeviceRemote"; }
    const char *port_count() const	{ return "-/0-1"; }
    const char *processing() const	{ return "a/h"; }
    /* x/y says the packets going out are not the same as the ones going in */
    const char *flow_code() const	{ return "x/y"; }
    const char *flags() const		{ return "S3"; }
    
    virtual int configure(Vector<String> &conf, ErrorHandler *);
    virtual int initialize(ErrorHandler *);
    virtual void cleanup(CleanupStage);
    
    void add_handlers();
    bool run_task(Task*); /* notifies there is something to pull */
    void selected(int fd, int mask);
    void push(int, Packet *p);

    int write_packet(Packet*, ErrorHandler *errh);

    bool isConnected() const;
    int get_pipefd(const bool write) const;
    int get_bufLen() const;
    int get_remoteSockFd() const;
    pthread_mutex_t* get_pipeMutex();

protected:
    Task _task;

private:

    static void* receiver_thread(void *arg);

    String _hostname;
    int _port;
    uint32_t _psid;
    uint8_t _channel;
    int8_t _txPower;
    uint8_t _dataRateIndex;
    uint8_t _userPrio;

    bool _setTstamp;
    int _bufLen;
    unsigned _headroom;     // headroom to build in

    NotifierSignal _signal;	// packet is available to pull()
    Packet *_wq;			// queue to store pulled packet

    pthread_t _rcvrTid;     // receiver thread id
    bool _isConnected;
    int _remoteSockFd;

    int _pipeFd[2];         // pipe file descriptors
    uint8_t* _pipeBuf; // buffer for reading from the pipe
    pthread_mutex_t _pipeMutex;
};

CLICK_ENDDECLS
#endif

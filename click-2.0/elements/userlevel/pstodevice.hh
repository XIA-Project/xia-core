#ifndef CLICK_PSTODEVICE_HH
#define CLICK_PSTODEVICE_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/task.hh>
#include <click/timer.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
 * =title PSToDevice
 * =c
 * PSToDevice(DEVNAME, QUEUE [, I<keywords>])
 * =s netdevices
 * sends packets to network device using Packet I/O Engine
 * =d
 *
 * Pulls packets and sends them out the named device using
 * Packet I/O Engine
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item BURST
 *
 * Integer. Number of packets to send to the device. Defaults to 64.
 *
 * =n
 *
 * Packets sent via PSToDevice should already have a link-level
 * header prepended. This means that ARP processing,
 * for example, must already have been done.
 */

class PSToDevice : public Element { public:

    PSToDevice();
    ~PSToDevice();

    const char *class_name() const		{ return "PSToDevice"; }
    const char *port_count() const		{ return "1/0"; }
    const char *processing() const		{ return "l/h"; }
    const char *flags() const			{ return "S2"; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    String ifname() const			{ return _ifname; }

    bool run_task(Task *);
    void selected(int, int);

    static String read_param(Element *e, void *thunk);

  private:
    Task _task;
    NotifierSignal _signal;
    
    String _ifname;
    unsigned int _queue_num;
    int _burst;

    struct ps_handle *_handle;
    struct ps_chunk *_chunk;
    int _in_chunk_next_idx;
    int _in_chunk_next_off;

    int _pulls;
};

CLICK_ENDDECLS
#endif

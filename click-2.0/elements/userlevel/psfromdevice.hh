#ifndef CLICK_PSFROMDEVICE_HH
#define CLICK_PSFROMDEVICE_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/task.hh>
CLICK_DECLS

/*
=title PSFromDevice

=c

PSFromDevice(DEVNAME, QUEUE, [, I<keywords>])

=s netdevices

reads packets from network device using Packet I/O Engine

=d

Reads packets from Packet I/O Engine that were received on the network controller
named DEVNAME.

Keyword arguments are:

=over 8

=item HEADROOM

Integer. Amount of bytes of headroom to leave before the packet data. Defaults
to roughly 28.

=item BURST

Integer. Number of packets to read per scheduling. Defaults to 64.

=back

=e

  PSFromDevice(eth0, 0) -> ...

*/

class PSFromDevice : public Element { public:

    PSFromDevice();
    ~PSFromDevice();

    const char *class_name() const	{ return "MQPollDevice"; }
    const char *port_count() const	{ return "0/1"; }
    const char *processing() const	{ return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    bool run_task(Task *);
    void selected(int fd, int mask);

  private:
    Task _task;

    String _ifname;
    unsigned int _queue_num;
    bool _promisc;
    unsigned int _headroom;
    int _burst;

    struct ps_handle *_handle;
    struct ps_queue *_queue;
    struct ps_chunk *_chunk;

    uint64_t _count;
    uint64_t _chunks;
    uint64_t _packets;
    uint64_t _bytes;

    static String read_handler(Element*, void*);
};

CLICK_ENDDECLS
#endif

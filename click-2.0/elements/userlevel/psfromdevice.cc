#include <click/config.h>
#include "psfromdevice.hh"
#include <click/error.hh>
#include <click/args.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
extern "C" {
#include <ps.h>
}

CLICK_DECLS

void destructor(unsigned char *, size_t);
PSFromDevice::PSFromDevice()
    : _task(this)
{
    _handle = NULL;
    _queue = NULL;
    _chunk = NULL;
    _count = 0;
    _chunks = 0;
    _packets = 0;
    _bytes = 0;
}

PSFromDevice::~PSFromDevice()
{
}

int
PSFromDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool promisc= false;
    _burst = 64;
    _headroom = Packet::default_headroom;
    _headroom += (4 - (_headroom + 2) % 4) % 4; // default 4/2 alignment
    if (Args(conf, this, errh)
	.read_mp("DEVNAME", _ifname)
	.read_mp("QUEUE", _queue_num)
	.read("BURST", _burst)
	.read("PROMISC", promisc)
	.read("HEADROOM", _headroom)
	.complete() < 0)
	return -1;
    if (!_ifname)
	return errh->error("interface not set");
    if (_burst <= 0 || _burst >= MAX_CHUNK_SIZE)
	return errh->error("bad BURST");
    if (_headroom > 8190)
	return errh->error("HEADROOM out of range");

    return 0;
}

int
PSFromDevice::initialize(ErrorHandler *errh)
{
    // check for duplicate readers
    void *&used = router()->force_attachment("device_reader_" + _ifname + "-" + String(_queue_num));
    if (used)
	return errh->error("duplicate reader for device %<%s-%u%>", _ifname.c_str(), _queue_num);


    // init handle
    _handle = new struct ps_handle;
    assert(_handle);

    int ret;
    ret = ps_init_handle(_handle);
    assert(!ret);

    // alloc queue
    _queue = new struct ps_queue;
    assert(_queue);

    // find dev
    struct ps_device devices[MAX_DEVICES];
    int num_devices = ps_list_devices(devices);
    assert(num_devices != -1);
    assert(num_devices > 0);
    _queue->ifindex = -1;
    for (int i = 0; i < num_devices; i++) {
        if (!strcmp(devices[i].name, _ifname.c_str())) {
            _queue->ifindex = i;
            break;
        }
    }
    assert(_queue->ifindex != -1);

    // set queue num
    _queue->qidx = _queue_num;

    // attach to a queue
    ret = ps_attach_rx_device(_handle, _queue);
    assert(!ret);

    // init chunk
    _chunk = new struct ps_chunk;
    assert(_chunk);

    ret = ps_alloc_chunk(_handle, _chunk);
    assert(!ret);


    _chunk->recv_blocking = 0;


    ScheduleInfo::join_scheduler(this, &_task, errh);
    return 0;
}

void
PSFromDevice::cleanup(CleanupStage)
{
    // the following message is disabled because %Lu generated assertion failure in ErrorHandler::vxformat()
    //click_chatter("PSFromDevice: %s-%u: count %Lu chunks %Lu packets %Lu bytes %Lu\n", _ifname.c_str(), _queue_num, _count, _chunks, _packets, _bytes);

    if (_chunk) {
        ps_free_chunk(_chunk);
        delete _chunk;
        _chunk = NULL;
    }
    if (_queue) {
        ps_detach_rx_device(_handle, _queue);
        delete _queue;
        _queue = NULL;
    }
    if (_handle) {
        ps_close_handle(_handle);
        delete _handle;
        _handle = NULL;
    }
}

void
PSFromDevice::selected(int, int)
{
    _task.reschedule();
}

bool
PSFromDevice::run_task(Task *)
{
    int ret;

    _count++;
    _task.fast_reschedule();

    _chunk->cnt = _burst;

    ret = ps_recv_chunk(_handle, _chunk);
    if (ret < 0) {
        if (errno == EINTR) {
            return false;
        }
        else if (!_chunk->recv_blocking && errno == EWOULDBLOCK) {
            return false;
        }
        else {
            assert(false);
            return false;
        }
    }

    _chunks++;
    for (int i = 0; i < ret; i++) {
        //Packet *p = Packet::make(_headroom, _chunk->buf + _chunk->info[i].offset, _chunk->info[i].len, 0);
	    if (_chunk->info[i].len>0) {
		    Packet *p = Packet::make((unsigned char *)(_chunk->buf + _chunk->info[i].offset), _chunk->info[i].len, &destructor);
		    output(0).push(p);
	    }
        _bytes += _chunk->info[i].len;
    }
    _packets += _chunk->cnt;

    return _chunk->cnt > 0;
}

void destructor(unsigned char *, size_t) 
{
    return;
}


String
PSFromDevice::read_handler(Element* e, void *thunk)
{
    PSFromDevice *fd = (PSFromDevice *)e;
    switch((uintptr_t) thunk) {
    case 0:
	return String(fd->_count);
    case 1:
	return String(fd->_chunks);
    case 2:
	return String(fd->_packets);
    case 3:
	return String(fd->_bytes);
    default:
	return String();
    }
}

void
PSFromDevice::add_handlers()
{
    add_read_handler("count", read_handler, (void *)0);
    add_read_handler("chunks", read_handler, (void *)1);
    add_read_handler("packets", read_handler, (void *)2);
    add_read_handler("bytes", read_handler, (void *)3);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(PSFromDevice PSFromDevice-PSFromDevice)
ELEMENT_LIBS(-lps)


#include <click/config.h>
#include "pstodevice.hh"
#include <click/error.hh>
#include <click/args.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
extern "C" {
#include <ps.h>
}

CLICK_DECLS

PSToDevice::PSToDevice()
    : _task(this)
{
    _handle = NULL;
    _chunk = NULL;
    _pulls = 0;
    _chunks = 0;
    _packets = 0;
    _bytes = 0;
}

PSToDevice::~PSToDevice()
{
}

int
PSToDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _burst = 64;
    if (Args(conf, this, errh)
	.read_mp("DEVNAME", _ifname)
	.read_mp("QUEUE", _queue_num)
	.read("BURST", _burst)
	.complete() < 0)
	return -1;
    if (!_ifname)
	return errh->error("interface not set");
    if (_burst <= 0 || _burst >= MAX_CHUNK_SIZE)
	return errh->error("bad BURST");

    return 0;
}

int
PSToDevice::initialize(ErrorHandler *errh)
{
    // check for duplicate writers
    void *&used = router()->force_attachment("device_writer_" + _ifname + "-" + String(_queue_num));
    if (used)
	return errh->error("duplicate writer for device %<%s-%u%>", _ifname.c_str(), _queue_num);


    // init handle
    _handle = new struct ps_handle;
    assert(_handle);

    int ret;
    ret = ps_init_handle(_handle);
    assert(!ret);

    // init chunk
    _chunk = new struct ps_chunk;
    assert(_chunk);

    ret = ps_alloc_chunk(_handle, _chunk);
    assert(!ret);
    assert(_chunk->info);

    // find dev
    struct ps_device devices[MAX_DEVICES];
    int num_devices = ps_list_devices(devices);
    assert(num_devices != -1);
    assert(num_devices > 0);
    _chunk->queue.ifindex = -1;
    for (int i = 0; i < num_devices; i++) {
        if (!strcmp(devices[i].name, _ifname.c_str())) {
            _chunk->queue.ifindex = i;
	    //click_chatter("%s ifindex %d", _ifname.c_str(), _chunk->queue.ifindex);
            break;
        }
    }
    assert(_chunk->queue.ifindex != -1);

    // set queue num
    _chunk->queue.qidx = _queue_num;

    _in_chunk_next_idx = 0;
    _in_chunk_next_off = 0;


    ScheduleInfo::join_scheduler(this, &_task, errh);
    _signal = Notifier::upstream_empty_signal(this, 0, &_task);
    return 0;
}

void
PSToDevice::cleanup(CleanupStage)
{
    // the following message is disabled because %Lu generated assertion failure in ErrorHandler::vxformat()
    //click_chatter("PSToDevice: %s-%u: pulls %Lu chunks %Lu packets %Lu bytes %Lu\n", _ifname.c_str(), _queue_num, _pulls, _chunks, _packets, _bytes);
    if (_chunk) {
        ps_free_chunk(_chunk);
        delete _chunk;
        _chunk = NULL;
    }
    if (_handle) {
        ps_close_handle(_handle);
        delete _handle;
        _handle = NULL;
    }
}


bool
PSToDevice::run_task(Task *)
{
    int ret;
    int count = 0;

    while (count < _burst) {
        _pulls++;
        Packet *p = input(0).pull();
        if (!p)
            break;
        count++;

        memcpy_aligned(_chunk->buf + _in_chunk_next_off, p->data(), p->length());
        _chunk->info[_in_chunk_next_idx].offset = _in_chunk_next_off;
        _chunk->info[_in_chunk_next_idx].len = p->length();

        _in_chunk_next_idx++;
        _in_chunk_next_off += ALIGN(p->length(), 64);
        _packets++;
        _bytes += p->length();

        if (_in_chunk_next_idx == _burst) {
            _chunk->cnt = _burst;
            ret = ps_send_chunk(_handle, _chunk);
            assert(ret >= 0);

            _in_chunk_next_idx = 0;
            _in_chunk_next_off = 0;
            _chunks++;
        }

        p->kill();
    }

    if (count == _burst || _signal)
        _task.fast_reschedule();

    return count > 0;
}

void
PSToDevice::selected(int, int)
{
    _task.reschedule();
}

String
PSToDevice::read_handler(Element *e, void *thunk)
{
    PSToDevice *td = (PSToDevice *)e;
    switch((uintptr_t) thunk) {
    case 0:
	return String(td->_pulls);
    case 1:
	return String(td->_chunks);
    case 2:
	return String(td->_packets);
    case 3:
	return String(td->_bytes);
    default:
	return String();
    }
}

void
PSToDevice::add_handlers()
{
    add_task_handlers(&_task);
    add_read_handler("pulls", read_handler, (void *)0);
    add_read_handler("chunks", read_handler, (void *)1);
    add_read_handler("packets", read_handler, (void *)2);
    add_read_handler("bytes", read_handler, (void *)3);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PSToDevice PSToDevice-PSToDevice)
ELEMENT_LIBS(-lps)


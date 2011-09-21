#include <click/config.h>
#include <click/glue.hh>
#include "isolatedcpuqueue.hh"
#include <click/error.hh>
#include <click/confparse.hh>

IsoCPUQueue::IsoCPUQueue()
  :  _capacity(0)
{
  memset(&_q, 0, sizeof(_q));
}

IsoCPUQueue::~IsoCPUQueue()
{
}

int
IsoCPUQueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
  unsigned new_capacity = 128;
  if (cp_va_kparse(conf, this, errh,
		   "CAPACITY", cpkP, cpInteger, &new_capacity,
		   cpEnd) < 0)
    return -1;
  _capacity = (new_capacity+7)/8*8-1;
  if (_capacity<=0) return -1;
  return 0;
}

int
IsoCPUQueue::initialize(ErrorHandler *errh)
{
  for (int i=0; i<NUM_CLICK_CPUS; i++) {
    if (!(_q[i]._q = new Packet*[_capacity+1]))
      return errh->error("out of memory!");
    _q[i]._drops = 0;
    _q[i]._head = _q[i]._tail = 0; 
  }
  return 0;
}

void
IsoCPUQueue::cleanup(CleanupStage)
{
  for (int i=0; i<NUM_CLICK_CPUS; i++) {
    for (int j = _q[i]._head; j != _q[i]._tail; j = next_i(j))
      _q[i]._q[j]->kill();
    delete[] _q[i]._q;
    _q[i]._q = 0;
  }
}

inline Packet *
IsoCPUQueue::deq(int n)
{
  if (_q[n]._head != _q[n]._tail) {
    Packet *p = _q[n]._q[_q[n]._head];
    _q[n]._head = next_i(_q[n]._head);
    //click_chatter("IsoCPUQueue CPU %d deq", n);
    return p;
  } else
    return NULL;
}

void
IsoCPUQueue::push(int, Packet *p)
{
#if HAVE_MULTITHREAD
#if CLICK_USERLEVEL
    int n = click_current_thread_id;
#else
    int n = click_current_processor();
#endif
#else
    int n = 0;
#endif
    int next = next_i(_q[n]._tail);
    if (next != _q[n]._head) {
	_q[n]._q[_q[n]._tail] = p;
	_q[n]._tail = next;
    } else {
	p->kill();
	_q[n]._drops++;
    }
}

Packet *
IsoCPUQueue::pull(int /*port*/)
{
    Packet *p = NULL;
#if HAVE_MULTITHREAD
#if CLICK_USERLEVEL
    int n = click_current_thread_id;
#else
    int n = click_current_processor();
#endif
#else
    int n = 0;
#endif
    p = deq(n);
    return p;
}

String
IsoCPUQueue::read_handler(Element *e, void *thunk)
{
  IsoCPUQueue *q = static_cast<IsoCPUQueue *>(e);
  switch (reinterpret_cast<intptr_t>(thunk)) {
   case 0:
    return String(q->capacity());
   case 1:
    return String(q->drops());
   default:
    return "";
  }
}

void
IsoCPUQueue::add_handlers()
{
  add_read_handler("capacity", read_handler, (void *)0);
  add_read_handler("drops", read_handler, (void *)1);
}

//ELEMENT_REQUIRES(linuxmodule)
EXPORT_ELEMENT(IsoCPUQueue)
ELEMENT_MT_SAFE(IsoCPUQueue)

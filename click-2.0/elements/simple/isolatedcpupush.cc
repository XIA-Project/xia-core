#include <click/config.h>
#include <click/glue.hh>
#include "isolatedcpupush.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>

IsoCPUPush::IsoCPUPush()
  :  _capacity(0)
{
  memset(&_q, 0, sizeof(_q));
}

IsoCPUPush::~IsoCPUPush()
{
}

int
IsoCPUPush::configure(Vector<String> &conf, ErrorHandler *errh)
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
IsoCPUPush::initialize(ErrorHandler *errh)
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
IsoCPUPush::cleanup(CleanupStage)
{
  for (int i=0; i<NUM_CLICK_CPUS; i++) {
    for (int j = _q[i]._head; j != _q[i]._tail; j = next_i(j))
      _q[i]._q[j]->kill();
    delete[] _q[i]._q;
    _q[i]._q = 0;
  }
}

inline Packet *
IsoCPUPush::deq(int n)
{
  if (_q[n]._head != _q[n]._tail) {
    Packet *p = _q[n]._q[_q[n]._head];
    asm __volatile__("": : :"memory");
    _q[n]._head = next_i(_q[n]._head);
    //click_chatter("IsoCPUPush CPU %d deq", n);
    return p;
  } else
    return NULL;
}


void
IsoCPUPush::push(int port, Packet *p)
{
    int n = port;
    int next = next_i(_q[n]._tail);

    if (next != _q[n]._head) {
	_q[n]._q[_q[n]._tail] = p;
        asm __volatile__("": : :"memory");
	_q[n]._tail = next;
    } else {
	p->kill();
	_q[n]._drops++;
	click_chatter("Drop IsoCPUPush");
    }

    if (port==0) {
	do {
	    bool ready = true;
	    for (int i=0;i<ninputs();i++) {
		if (is_empty(i)) ready = false;
	    }
	    if (!ready) return;

	    Packet *pout = NULL;
	    for (int i=0;i<ninputs();i++)  {
		Packet *current = deq(i);
		if (!current) click_chatter("is_empty %d", is_empty(i));
		if (pout==NULL && PAINT_ANNO(current)!=10) 
		    pout = current;
		else
		    current->kill();
	    }
	    if (pout) output(0).push(pout);
	} while (true);
    }

}

String
IsoCPUPush::read_handler(Element *e, void *thunk)
{
  IsoCPUPush *q = static_cast<IsoCPUPush *>(e);
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
IsoCPUPush::add_handlers()
{
  add_read_handler("capacity", read_handler, (void *)0);
  add_read_handler("drops", read_handler, (void *)1);
}

//ELEMENT_REQUIRES(linuxmodule)
EXPORT_ELEMENT(IsoCPUPush)
ELEMENT_MT_SAFE(IsoCPUPush)

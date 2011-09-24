#include <click/config.h>
#include <click/glue.hh>
#include "barrier.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/standard/scheduleinfo.hh>

CLICK_DECLS
Barrier::Barrier() :_task(this), _current_port(0), _p(NULL)
{

}

Barrier::~Barrier()
{

}

/*
int
Barrier::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int ports = 0;
  if (cp_va_kparse(conf, this, errh,
		   "PORTS", cpkP+cpkM, cpInteger, &ports,
		   cpEnd) < 0)
      return -1;
  _ports = ports;
  return 0;
}
*/

int
Barrier::initialize(ErrorHandler *errh)
{
    ScheduleInfo::join_scheduler(this, &_task, errh);
    _p = new Packet*[ninputs()];
    return 0;
}

#define MAX_PORT 8

bool
Barrier::run_task(Task *)
{
    int cnt=0;

    _task.fast_reschedule();

    for (int i=_current_port;i<ninputs();i++)  {
        _p[i] = input(i).pull();
	if (_p[i]==NULL) return cnt;
	_current_port++;
	cnt++;
    }

    Packet *pout = NULL;
    for (int i=0;i<ninputs();i++)  {
    	if (pout==NULL && PAINT_ANNO(_p[i])!=10) 
	    pout = _p[i];
	 else
	    _p[i]->kill();
    }
    _current_port = 0;
    if (pout) output(0).push(pout);

    return (cnt>0) ;
}

void
Barrier::selected(int, int)
{
    _task.reschedule();
}

EXPORT_ELEMENT(Barrier)
CLICK_ENDDECLS

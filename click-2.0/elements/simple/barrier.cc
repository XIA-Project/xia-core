#include <click/config.h>
#include <click/glue.hh>
#include "barrier.hh"
#include <click/error.hh>
#include <click/confparse.hh>

CLICK_DECLS
Barrier::Barrier() :_task(this)
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

bool
Barrier::run_task(Task *)
{
    int cnt=0;
    Packet *p_old, *p;
    p = p_old = NULL;
    for (int i=0;i<ninputs();i++)  {
    	if (p_old) p_old->kill();
        p_old = p;
        p = input(i).pull();
 	assert((p_old==NULL) || p_old!=p);
	if (p) cnt++;
    }
    if (p_old) p_old->kill();
    if (p) output(0).push(p);

    return (cnt>0) ;
}

void
Barrier::selected(int, int)
{
    _task.reschedule();
}

EXPORT_ELEMENT(Barrier)
CLICK_ENDDECLS

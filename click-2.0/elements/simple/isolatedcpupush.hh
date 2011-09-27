#ifndef ISOCPUPush_HH
#define ISOCPUPush_HH

#include <click/element.hh>

class IsoCPUPush : public Element {
  struct _qstruct {
    Packet **_q;
    int _head;
    int _tail;
    uint64_t _drops;
  }
#if CLICK_LINUXMODULE
 ____cacheline_aligned_in_smp;
#else
  __attribute__ ((aligned (64)));
#endif
  struct _qstruct _q[NUM_CLICK_CPUS];

  int _capacity;

  int next_i(int i) const { return (i!=_capacity ? i+1 : 0); }
  int prev_i(int i) const { return (i!=0 ? i-1 : _capacity); }

  bool is_full(int n) const { int next = next_i(_q[n]._tail); return (next == _q[n]._head); }
  bool is_empty(int n) const { return (_q[n]._head == _q[n]._tail); }

  Packet *deq(int);
  static String read_handler(Element *, void *);

 public:

  IsoCPUPush();
  ~IsoCPUPush();

  const char *class_name() const	{ return "IsoCPUPush"; }
  const char *port_count() const	{ return "1-/1"; }
  const char *processing() const	{ return PUSH; }

  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);

  unsigned drops() const	{ uint64_t drops=0; for (int i=0;i<NUM_CLICK_CPUS;i++) { drops+=_q[i]._drops;} return drops;}
  unsigned capacity() const	{ return _capacity; }

  int configure(Vector<String> &, ErrorHandler *);

  void push(int port, Packet *);

  void add_handlers();
};

#endif

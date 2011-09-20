#ifndef ISOCPUQUEUE_HH
#define ISOCPUQUEUE_HH

/*
 * =c
 * IsoCPUQueue
 * IsoCPUQueue(CAPACITY)
 * =s smpclick
 * stores packets in FIFO queues.
 * =d
 *
 * Stores incoming packets in a first-in-first-out queue. Each CPU has its own
 * queue. The incoming packet is always enqueued onto the queue of the CPU
 * calling the push method. Drops incoming packets if the queue already holds
 * CAPACITY packets. The default for CAPACITY is 128.
 *
 * =a Queue
 */

#include <click/element.hh>

class IsoCPUQueue : public Element {
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
  Packet *deq(int);

  static String read_handler(Element *, void *);

 public:

  IsoCPUQueue();
  ~IsoCPUQueue();

  const char *class_name() const		{ return "IsoCPUQueue"; }
  const char *port_count() const		{ return "1/1-"; }
  const char *processing() const		{ return PUSH_TO_PULL; }
  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);

  unsigned drops() const			{ uint64_t drops=0; for (int i=0;i<NUM_CLICK_CPUS;i++) { drops+=_q[i]._drops;} return drops;}
  unsigned capacity() const			{ return _capacity; }

  int configure(Vector<String> &, ErrorHandler *);

  void push(int port, Packet *);
  Packet *pull(int port);

  void add_handlers();
};

#endif

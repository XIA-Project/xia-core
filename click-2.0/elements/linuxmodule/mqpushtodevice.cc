// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * mqtodevice.{cc,hh} -- element sends packets to Linux devices.
 * Robert Morris
 * Eddie Kohler: register once per configuration
 * Benjie Chen: polling
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001 International Computer Science Institute
 * Copyright (c) 2005-2007 Regents of the University of California
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/glue.hh>
#include "mqpolldevice.hh"
#include "mqpushtodevice.hh"
#include <click/error.hh>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <net/pkt_sched.h>
#if __i386__
#include <asm/msr.h>
#endif
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

/* for watching when devices go offline */
static AnyDeviceMap to_device_map;
static struct notifier_block device_notifier;
extern "C" {
static int device_notifier_hook(struct notifier_block *nb, unsigned long val, void *v);
#if HAVE_CLICK_KERNEL_TX_NOTIFY
static struct notifier_block tx_notifier;
static int registered_tx_notifiers;
static int tx_notifier_hook(struct notifier_block *nb, unsigned long val, void *v);
#endif
}

void
MQPushToDevice::static_initialize()
{
    to_device_map.initialize();
#if HAVE_CLICK_KERNEL_TX_NOTIFY
    tx_notifier.notifier_call = tx_notifier_hook;
    tx_notifier.priority = 1;
    tx_notifier.next = 0;
#endif
    device_notifier.notifier_call = device_notifier_hook;
    device_notifier.priority = 1;
    device_notifier.next = 0;
    register_netdevice_notifier(&device_notifier);

}

void
MQPushToDevice::static_cleanup()
{
    unregister_netdevice_notifier(&device_notifier);
#if HAVE_CLICK_KERNEL_TX_NOTIFY
    if (registered_tx_notifiers)
	unregister_net_tx(&tx_notifier);
#endif
}

inline void
MQPushToDevice::tx_wake_queue(net_device *dev) 
{
    //click_chatter("%{element}::%s for dev %s\n", this, __func__, dev->name);
}

#if HAVE_CLICK_KERNEL_TX_NOTIFY
extern "C" {
static int
tx_notifier_hook(struct notifier_block *nb, unsigned long val, void *v) 
{
    struct net_device *dev = (struct net_device *)v;
    if (!dev) {
	return 0;
    }
    bool down = true;
    unsigned long lock_flags;
    to_device_map.lock(false, lock_flags);
    AnyDevice *es[8];
    int nes = to_device_map.lookup_all(dev, down, es, 8);
    for (int i = 0; i < nes; i++) 
	((MQPushToDevice *)(es[i]))->tx_wake_queue(dev);
    to_device_map.unlock(false, lock_flags);
    return 0;
}
}
#endif

MQPushToDevice::MQPushToDevice()
    : _dev_idle(0), _rejected(0), _hard_start(0), _no_pad(false)
{
}

MQPushToDevice::~MQPushToDevice()
{
}


int
MQPushToDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _burst = 16;
    _queue = 0;
    if (AnyDevice::configure_keywords(conf, errh, false) < 0
	|| cp_va_kparse(conf, this, errh,
			"DEVNAME", cpkP+cpkM, cpString, &_devname,
			"QUEUE", cpkP, cpUnsigned, &_queue,
			"BURST", cpkP, cpUnsigned, &_burst,
			"NO_PAD", 0, cpBool, &_no_pad,
			cpEnd) < 0)
	return -1;
    //return find_device(&to_device_map, errh);
    net_device* dev = lookup_device(errh);
    if (!dev) return -1;
    set_device(dev, &to_device_map, 0);
    return 0;
}

int
MQPushToDevice::initialize(ErrorHandler *errh)
{
    _capacity = _burst;
    _q.q = new Packet*[_capacity+1];
    memset(_q.q, 0, (sizeof(Packet*) * (_capacity+1)));
    _q.head = _q.tail = 0;
    if (AnyDevice::initialize_keywords(errh) < 0)
	return -1;
    
//#ifndef HAVE_CLICK_KERNEL
//    errh->warning("not compiled for a Click kernel");
//#endif

    // check for duplicate writers
    if (ifindex() >= 0) {
	void *&used = router()->force_attachment("device_writer_" + String(ifindex()) + "_queue_" + String(_queue));
	if (used)
	    return errh->error("duplicate writer for device '%s'", _devname.c_str());
	used = this;
    }

#if HAVE_CLICK_KERNEL_TX_NOTIFY
    if (!registered_tx_notifiers) {
	tx_notifier.next = 0;
	register_net_tx(&tx_notifier);
    }
    registered_tx_notifiers++;
#endif

    reset_counts();
    return 0;
}

void
MQPushToDevice::reset_counts()
{
  _npackets = 0;
  
  _busy_returns = 0;
  _too_short = 0;
  _runs = 0;
  _pulls = 0;
#if CLICK_DEVICE_STATS
  _activations = 0;
  _time_clean = 0;
  _time_freeskb = 0;
  _time_queue = 0;
  _perfcnt1_pull = 0;
  _perfcnt1_clean = 0;
  _perfcnt1_freeskb = 0;
  _perfcnt1_queue = 0;
  _perfcnt2_pull = 0;
  _perfcnt2_clean = 0;
  _perfcnt2_freeskb = 0;
  _perfcnt2_queue = 0;
#endif
#if CLICK_DEVICE_THESIS_STATS || CLICK_DEVICE_STATS
  _pull_cycles = 0;
#endif
}

void
MQPushToDevice::cleanup(CleanupStage stage)
{
    if (_dev && _dev->is_polling(_dev, _queue + 1024) > 0)
        _dev->poll_off(_dev, _queue + 1024);

#if HAVE_CLICK_KERNEL_TX_NOTIFY
    if (stage >= CLEANUP_INITIALIZED) {
	registered_tx_notifiers--;
	if (registered_tx_notifiers == 0)
	    unregister_net_tx(&tx_notifier);
    }
#endif
    clear_device(&to_device_map, 0);
}

Packet * 
MQPushToDevice::peek()
{
  if (_q.head != _q.tail) {
    Packet *p = _q.q[_q.head];
    return p;
  } else
    return NULL;
}

Packet * 
MQPushToDevice::deq()
{
  if (_q.head != _q.tail) {
    Packet *p = _q.q[_q.head];
    _q.q[_q.head] = NULL;
    _q.head = next_i(_q.head);
    return p;
  } else
    return NULL;

}

bool MQPushToDevice::enq(Packet *p)
{
    int next = next_i(_q.tail);
    if (next != _q.head) {
	_q.q[_q.tail] = p;
	_q.tail = next;
	return true;
    } else 
	return false;

}

/*
 * Problem: Linux drivers aren't required to
 * accept a packet even if they've marked themselves
 * as idle. What do we do with a rejected packet?
 */

#if LINUX_VERSION_CODE < 0x020400
# define netif_queue_stopped(dev)	((dev)->tbusy)
# define netif_wake_queue(dev)		mark_bh(NET_BH)
#endif

void
MQPushToDevice::push(int port, Packet *p)
{
    int busy = 0;
    int sent = 0;

    _runs++;

#if CLICK_DEVICE_STATS
    unsigned low00, low10;
    uint64_t time_now;
    SET_STATS(low00, low10, time_now);
#endif
    
    if (p==NULL) return;

#if HAVE_LINUX_MQ_POLLING
    bool is_polling = (_dev->is_polling(_dev, _queue + 1024) > 0);
    struct sk_buff *clean_skbs;
    if (is_polling)
	clean_skbs = _dev->mq_tx_clean(_dev, _queue);
    else {
        if (_dev->poll_on(_dev, _queue + 1024) != 0) {
            p->kill();
            return;
        }
	//clean_skbs = 0;
    }
#endif

    if (!enq(p))  {
        struct sk_buff *skb1 = p->skb();
	kfree_skb(skb1);
    }
     
    if (size()>=_burst)  {
	    while (sent < _burst) {
#if CLICK_DEVICE_THESIS_STATS && !CLICK_DEVICE_STATS
		    click_cycles_t before_pull_cycles = click_get_cycles();
#endif
		    p = deq();
		    if (p==NULL) {
                        break;  // this should not happen usually
		    }
		    _npackets++;

		    GET_STATS_RESET(low00, low10, time_now, 
				    _perfcnt1_pull, _perfcnt2_pull, _pull_cycles);

		    busy = queue_packet(p);

		    GET_STATS_RESET(low00, low10, time_now, 
				    _perfcnt1_queue, _perfcnt2_queue, _time_queue);

		    if (busy) {
			    break;
		    }
		    sent++;

#if HAVE_LINUX_MQ_POLLING
                // eob is handled by driver automatically
		//    if (is_polling && _npackets % _burst == 0)
		//	    _dev->mq_tx_eob(_dev, _queue);
#endif
	    }
    }
#if HAVE_LINUX_MQ_POLLING
    if (is_polling) {
	// 8.Dec.07: Do not recycle skbs until after unlocking the device, to
	// avoid deadlock.  After initial patch by Joonwoo Park.
	if (clean_skbs) {
# if CLICK_DEVICE_STATS
	    if (_activations > 1)
		GET_STATS_RESET(low00, low10, time_now, 
				_perfcnt1_clean, _perfcnt2_clean, _time_clean);
# endif
	    skbmgr_recycle_skbs(clean_skbs);
# if CLICK_DEVICE_STATS
	    if (_activations > 1)
		GET_STATS_RESET(low00, low10, time_now, 
				_perfcnt1_freeskb, _perfcnt2_freeskb, _time_freeskb);
# endif
	}
#endif
    }
}

int
MQPushToDevice::queue_packet(Packet *p)
{
    struct sk_buff *skb1 = p->skb();
  
    /*
     * Ensure minimum ethernet packet size (14 hdr + 46 data).
     * I can't figure out where Linux does this, so I don't
     * know the correct procedure.
     */
    if (!_no_pad && skb1->len < 60) {
	if (skb_tailroom(skb1) < 60 - skb1->len) {
	    if (++_too_short == 1)
		printk("<1>MQPushToDevice %s packet too small (len %d, tailroom %d), had to copy\n", skb1->len, skb_tailroom(skb1));
	    struct sk_buff *nskb = skb_copy_expand(skb1, skb_headroom(skb1), skb_tailroom(skb1) + 60 - skb1->len, GFP_ATOMIC);
            //p->kill();
	    kfree_skb(skb1);
	    //skbmgr_recycle_skbs(skb1);
	    if (!nskb)
		return -1;
	    skb1 = nskb;
	}
	skb_put(skb1, 60 - skb1->len);
    }

    // set the device annotation;
    // apparently some devices in Linux 2.6 require it
    skb1->dev = _dev;
    
    int ret;
#if HAVE_LINUX_MQ_POLLING
    if (_dev->is_polling(_dev, _queue + 1024) > 0)
	ret = _dev->mq_tx_queue(_dev, _queue, skb1);
    else
#endif
	{
	    //ret = _dev->hard_start_xmit(skb1, _dev); 
	    /* if a packet is sent in non-polling mode the ported ixgbe driver dies */
	    if (_hard_start++ ==1)
	        printk("<1>MQPushToDevice %s polling not enabled! Dropping packet\n", _dev->name);
	    kfree_skb(skb1);
            //p->kill();
	    //skbmgr_recycle_skbs(skb1);
	    return 0;
	}
    if (ret != 0) {
	if (++_rejected == 1)
	    printk("<1>MQPushToDevice %s rejected a packet!\n", _dev->name);
	kfree_skb(skb1);
        //p->kill();
        //skbmgr_recycle_skbs(skb1);
    }
    return ret;
}

void
MQPushToDevice::change_device(net_device *dev)
{
    set_device(dev, &to_device_map, true);
}

extern "C" {
static int
device_notifier_hook(struct notifier_block *nb, unsigned long flags, void *v)
{
#ifdef NETDEV_GOING_DOWN
    if (flags == NETDEV_GOING_DOWN)
	flags = NETDEV_DOWN;
#endif
    if (flags == NETDEV_DOWN || flags == NETDEV_UP || flags == NETDEV_CHANGE) {
	bool down = (flags == NETDEV_DOWN);
	net_device *dev = (net_device *)v;
	unsigned long lock_flags;
	to_device_map.lock(true, lock_flags);
	AnyDevice *es[8];
	int nes = to_device_map.lookup_all(dev, down, es, 8);
	for (int i = 0; i < nes; i++)
	    ((MQPushToDevice *)(es[i]))->change_device(down ? 0 : dev);
	to_device_map.unlock(true, lock_flags);
    }
    return 0;
}
}

static String
MQPushToDevice_read_calls(Element *f, void *)
{
    MQPushToDevice *td = (MQPushToDevice *)f;
    return
	String(td->_rejected) + " packets rejected\n" +
	String(td->_hard_start) + " hard start xmit\n" +
	String(td->_busy_returns) + " device busy returns\n" +
	String(td->_npackets) + " packets sent\n" +
	String(td->_runs) + " calls to run_task()\n" +
	String(td->_pulls) + " pulls\n" +
#if CLICK_DEVICE_STATS
	String(td->_pull_cycles) + " cycles pull\n" +
	String(td->_time_clean) + " cycles clean\n" +
	String(td->_time_freeskb) + " cycles freeskb\n" +
	String(td->_time_queue) + " cycles queue\n" +
	String(td->_perfcnt1_pull) + " perfctr1 pull\n" +
	String(td->_perfcnt1_clean) + " perfctr1 clean\n" +
	String(td->_perfcnt1_freeskb) + " perfctr1 freeskb\n" +
	String(td->_perfcnt1_queue) + " perfctr1 queue\n" +
	String(td->_perfcnt2_pull) + " perfctr2 pull\n" +
	String(td->_perfcnt2_clean) + " perfctr2 clean\n" +
	String(td->_perfcnt2_freeskb) + " perfctr2 freeskb\n" +
	String(td->_perfcnt2_queue) + " perfctr2 queue\n" +
	String(td->_activations) + " transmit activations\n"
#else
	String()
#endif
	;
}

enum { H_COUNT, H_DROPS, H_PULL_CYCLES, H_TIME_QUEUE, H_TIME_CLEAN };

static String
MQPushToDevice_read_stats(Element *e, void *thunk)
{
    MQPushToDevice *td = (MQPushToDevice *)e;
    switch ((uintptr_t) thunk) {
      case H_COUNT:
	return String(td->_npackets);
      case H_DROPS:
	return String(td->_rejected);
#if CLICK_DEVICE_THESIS_STATS || CLICK_DEVICE_STATS
      case H_PULL_CYCLES:
	return String(td->_pull_cycles);
#endif
#if CLICK_DEVICE_STATS
      case H_TIME_QUEUE:
	return String(td->_time_queue);
      case H_TIME_CLEAN:
	return String(td->_time_clean);
#endif
      default:
	return String();
    }
}

static int
MQPushToDevice_write_stats(const String &, Element *e, void *, ErrorHandler *)
{
  MQPushToDevice *td = (MQPushToDevice *)e;
  td->reset_counts();
  return 0;
}

void
MQPushToDevice::add_handlers()
{
    add_read_handler("calls", MQPushToDevice_read_calls, 0);
    add_read_handler("count", MQPushToDevice_read_stats, (void *)H_COUNT);
    add_read_handler("drops", MQPushToDevice_read_stats, (void *)H_DROPS);
    // XXX deprecated
    add_read_handler("packets", MQPushToDevice_read_stats, (void *)H_COUNT);
#if CLICK_DEVICE_THESIS_STATS || CLICK_DEVICE_STATS
    add_read_handler("pull_cycles", MQPushToDevice_read_stats, (void *)H_PULL_CYCLES);
#endif
#if CLICK_DEVICE_STATS
    add_read_handler("enqueue_cycles", MQPushToDevice_read_stats, (void *)H_TIME_QUEUE);
    add_read_handler("clean_dma_cycles", MQPushToDevice_read_stats, (void *)H_TIME_CLEAN);
#endif
    add_write_handler("reset_counts", MQPushToDevice_write_stats, 0, Handler::BUTTON);
}

ELEMENT_REQUIRES(AnyDevice linuxmodule)
EXPORT_ELEMENT(MQPushToDevice)

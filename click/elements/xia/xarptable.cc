/*
 * arptable.{cc,hh} -- XARP resolver element
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2005 Regents of the University of California
 * Copyright (c) 2008-2009 Meraki, Inc.
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

// Modified from original ARP table code

#include <click/config.h>
#include "xarpquerier.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/args.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/xid.hh>
CLICK_DECLS

XARPTable::XARPTable()
    : _entry_capacity(0), _packet_capacity(2048), _expire_timer(this)
{
    _entry_count = _packet_count = _drops = 0;
}

XARPTable::~XARPTable()
{
}

int
XARPTable::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Timestamp timeout(300);
    if (Args(conf, this, errh)
        .read("CAPACITY", _packet_capacity)
        .read("ENTRY_CAPACITY", _entry_capacity)
        .read("TIMEOUT", timeout)
        .complete() < 0)
        return -1;
    set_timeout(timeout);
    if (_timeout_j) {
        _expire_timer.initialize(this);
        _expire_timer.schedule_after_sec(_timeout_j / CLICK_HZ);
    }
    return 0;
}

void
XARPTable::cleanup(CleanupStage)
{
    clear();
}

void
XARPTable::clear()
{
    // Walk the arp cache table and free any stored packets and arp entries.
    for (Table::iterator it = _table.begin(); it; ) {
        XARPEntry *ae = _table.erase(it);
        while (Packet *p = ae->_head) {
            ae->_head = p->next();
            p->kill();
            ++_drops;
        }
        _alloc.deallocate(ae);
    }
    _entry_count = _packet_count = 0;
    _age.__clear();
}

void
XARPTable::take_state(Element *e, ErrorHandler *errh)
{
    XARPTable *xarpt = (XARPTable *)e->cast("XARPTable");
    if (!xarpt)
        return;
    if (_table.size() > 0) {
        errh->error("late take_state");
        return;
    }

    _table.swap(xarpt->_table);
    _age.swap(xarpt->_age);
    _entry_count = xarpt->_entry_count;
    _packet_count = xarpt->_packet_count;
    _drops = xarpt->_drops;
    _alloc.swap(xarpt->_alloc);

    xarpt->_entry_count = 0;
    xarpt->_packet_count = 0;
}

void
XARPTable::slim(click_jiffies_t now)
{
    XARPEntry *ae;

    // Delete old entries.
    while ((ae = _age.front())
           && (ae->expired(now, _timeout_j)
           || (_entry_capacity && _entry_count > _entry_capacity))
           && ae->_perm == false) {
        _table.erase(ae->_xid);
        _age.pop_front();

        while (Packet *p = ae->_head) {
            ae->_head = p->next();
            p->kill();
            --_packet_count;
            ++_drops;
        }

        _alloc.deallocate(ae);
        --_entry_count;
    }

    // Mark entries for polling, and delete packets to make space.
    while (_packet_capacity && _packet_count > _packet_capacity) {
        while (ae->_head && _packet_count > _packet_capacity) {
            Packet *p = ae->_head;
            if (!(ae->_head = p->next()))
                ae->_tail = 0;
            p->kill();
            --_packet_count;
            ++_drops;
        }
        ae = ae->_age_link.next();
    }
}

void
XARPTable::run_timer(Timer *timer)
{
    // Expire any old entries, and make sure there's room for at least one
    // packet.
    _lock.acquire_write();
    slim(click_jiffies());
    _lock.release_write();
    if (_timeout_j)
        timer->schedule_after_sec(_timeout_j / CLICK_HZ + 1);
}

XARPTable::XARPEntry *
XARPTable::ensure(XID xid, click_jiffies_t now)
{
    _lock.acquire_write();
    Table::iterator it = _table.find(xid);
    if (!it) {
        void *x = _alloc.allocate();
        if (!x) {
            _lock.release_write();
            return 0;
        }

        ++_entry_count;
        if (_entry_capacity && _entry_count > _entry_capacity)
            slim(now);

        XARPEntry *ae = new(x) XARPEntry(xid);
        ae->_live_at_j = now;
        ae->_polled_at_j = ae->_live_at_j - CLICK_HZ;
        _table.set(it, ae);

        _age.push_back(ae);
    }
    return it.get();
}

int
XARPTable::insert(XID xid, bool perm, const EtherAddress &eth, Packet **head)
{
    click_jiffies_t now = click_jiffies();
    XARPEntry *ae = ensure(xid, now);
    if (!ae)
        return -ENOMEM;

    ae->_eth = eth;
    ae->_known = !eth.is_broadcast();
        ae->_perm = perm;

    ae->_live_at_j = now;
    ae->_polled_at_j = ae->_live_at_j - CLICK_HZ;

    if (ae->_age_link.next()) {
        _age.erase(ae);
        _age.push_back(ae);
    }

    if (head) {
        *head = ae->_head;
        ae->_head = ae->_tail = 0;
        for (Packet *p = *head; p; p = p->next())
            --_packet_count;
    }

    _table.balance();
    _lock.release_write();
    return 0;
}

int
XARPTable::append_query(XID xid, Packet *p)
{
    click_jiffies_t now = click_jiffies();
    XARPEntry *ae = ensure(xid, now);
    if (!ae)
        return -ENOMEM;

    if (ae->known(now, _timeout_j)) {
        _lock.release_write();
        return -EAGAIN;
    }

    // Since we're still trying to send to this address, keep the entry just
    // this side of expiring.  This fixes a bug reported 5 Nov 2009 by Seiichi
    // Tetsukawa, and verified via testie, where the slim() below could delete
    // the "ae" XARPEntry when "ae" was the oldest entry in the system.
    if (_timeout_j) {
        click_jiffies_t live_at_j_min = now - _timeout_j;
        if (click_jiffies_less(ae->_live_at_j, live_at_j_min)) {
            ae->_live_at_j = live_at_j_min;
            // Now move "ae" to the right position in the list by walking
            // forward over other elements (potentially expensive?).
            XARPEntry *ae_next = ae->_age_link.next(), *next = ae_next;
            while (next && click_jiffies_less(next->_live_at_j, ae->_live_at_j))
                next = next->_age_link.next();
            if (ae_next != next) {
                _age.erase(ae);
                _age.insert(next /* might be null */, ae);
            }
        }
    }

    ++_packet_count;
    if (_packet_capacity && _packet_count > _packet_capacity)
        slim(now);

    if (ae->_tail)
        ae->_tail->set_next(p);
    else
        ae->_head = p;
    ae->_tail = p;
    p->set_next(0);

    int r;
    if (!click_jiffies_less(now, ae->_polled_at_j + CLICK_HZ / 10)) {
        ae->_polled_at_j = now;
        r = 1;
    } else
        r = 0;

    _table.balance();
    _lock.release_write();
    return r;
}

XID
XARPTable::reverse_lookup(const EtherAddress &eth)
{
    _lock.acquire_read();

    XID xid;
    for (Table::iterator it = _table.begin(); it; ++it)
        if (it->_eth == eth) {
            xid = it->_xid;
            break;
        }

    _lock.release_read();
    return xid;
}

String
XARPTable::read_handler(Element *e, void *user_data)
{
        XARPTable *xarpt = (XARPTable *) e;
        StringAccum sa;
        click_jiffies_t now = click_jiffies();

        switch (reinterpret_cast<uintptr_t>(user_data)) {
        case h_table:
                for (XARPEntry *ae = xarpt->_age.front(); ae; ae = ae->_age_link.next()) {
                                int ok = ae->known(now, xarpt->_timeout_j);

                                if (ok) {
                                        sa  << ae->_xid << ' ' << ae->_eth << ' '
                                                << Timestamp::make_jiffies(now - ae->_live_at_j) << ' '
                                                << (ae->_perm ? 1 : 0)
                                                << '\n';
                                }
                }
                break;
        }
        return sa.take_string();
}

int
XARPTable::write_handler(const String &str, Element *e, void *user_data, ErrorHandler *errh)
{
    XARPTable *xarpt = (XARPTable *) e;
    switch (reinterpret_cast<uintptr_t>(user_data)) {
      case h_insert: {
          XID xid;
          EtherAddress eth;
          bool perm;
          if (Args(xarpt, errh).push_back_words(str)
              .read_mp("XID", xid)
              .read_mp("ETH", eth)
                  .read_mp("STATIC", perm)
              .complete() < 0)
              return -1;
          xarpt->insert(xid, perm, eth);
          return 0;
      }
      case h_delete: {
          XID xid;
          if (Args(xarpt, errh).push_back_words(str)
              .read_mp("XID", xid)
              .complete() < 0)
              return -1;
          xarpt->insert(xid, false, EtherAddress::make_broadcast()); // XXX?
          return 0;
      }
      case h_clear:
        xarpt->clear();
        return 0;
      default:
        return -1;
    }
}

void
XARPTable::add_handlers()
{
    add_read_handler("table", read_handler, h_table);
    add_data_handlers("drops", Handler::OP_READ, &_drops);
    add_data_handlers("count", Handler::OP_READ, &_entry_count);
    add_data_handlers("length", Handler::OP_READ, &_packet_count);
    add_write_handler("insert", write_handler, h_insert);
    add_write_handler("delete", write_handler, h_delete);
    add_write_handler("clear", write_handler, h_clear);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XARPTable)
ELEMENT_MT_SAFE(XARPTable)

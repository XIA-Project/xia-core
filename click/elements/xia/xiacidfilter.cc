#include <click/config.h>
#include "xiacidfilter.hh"
#include <click/timer.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/xiaheader.hh>
#include <iostream>
#include "xlog.hh"

CLICK_DECLS

#define CLEANUP_INTERVAL 600
#define MAX_AGE 3600

XIACidFilter::XIACidFilter(): _cleanup_timer(this)
{
}

XIACidFilter::~XIACidFilter()
{
}

int XIACidFilter::configure(Vector<String> &conf, ErrorHandler *errh)
{
	bool _enable;

	if (cp_va_kparse(conf, this, errh,
					"ENABLE", cpkP + cpkM, cpBool, &_enable,
					cpEnd) < 0) {

		return -1;
	}

	enabled = _enable;
	_cleanup_timer.initialize(this);
	_cleanup_timer.schedule_after_sec(CLEANUP_INTERVAL);
	return 0;
}

void XIACidFilter::run_timer(Timer *t)
{
	HashTable<String, Timestamp>::iterator it;
	Timestamp now = Timestamp::now();

	DBG("XIACidFilter:: Blacklist cleanup %zu entries\n",
			_blacklist.size());
	for (it=_blacklist.begin(); it!=_blacklist.end();) {
		if (now - it->second > Timestamp::make_sec(MAX_AGE)) {
			DBG("XIACidFilter: Blacklist remove %s\n", it->first.c_str());
			it = _blacklist.erase(it);
		} else {
			++it;
		}
	}

	t->reschedule_after_sec(CLEANUP_INTERVAL);
}

int XIACidFilter::blacklisted(String id)
{
	if(_blacklist.get(id)) {
		return 1;
	}
	return 0;
}

void XIACidFilter::handleNetworkPacket(Packet *p)
{
	WritablePacket *pIn = p->uniqueify();

	struct click_xia *xiah = pIn->xia_header();

	Graph dst_dag, src_dag;
	dst_dag.from_wire_format(xiah->dnode, &xiah->node[0]);
	src_dag.from_wire_format(xiah->snode, &xiah->node[xiah->dnode]);

	Node src_intent = src_dag.get_final_intent();
	Node dst_intent = dst_dag.get_final_intent();
	std::string session_id = src_intent.to_string() + dst_intent.to_string();

	if(blacklisted(session_id.c_str())) {
		DBG("XIACidFilter: blacklisted session. Drop packet\n");
		return;
	}
	DBG("CID FILTER sending content packet to xcache\n");
	checked_output_push(PORT_OUT_XCACHE, pIn);
}

void XIACidFilter::blacklist(Packet *p)
{
	std::string blacklist_id;
	blacklist_id.assign((const char *)p->data(), (const char *)p->end_data());
	DBG("XIACidFilter: Blacklisting:%s:\n", blacklist_id.c_str());
	_blacklist[blacklist_id.c_str()] = Timestamp::now();
}

void XIACidFilter::handleXcachePacket(Packet *p)
{
	blacklist(p);
	//INFO("CID FILTER Packet received from xcache\n");
}

void XIACidFilter::push(int port, Packet *p)
{
	if (enabled) {
		switch(port) {
		case PORT_IN_XCACHE:
			handleXcachePacket(p);
			break;
		case PORT_IN_NETWORK:
			handleNetworkPacket(p);
			break;
		default:
			ERROR("Should not happen\n");
		}
	}
	p->kill();
}

int XIACidFilter::toggle(const String &conf, Element *e, void * /*vparam*/, ErrorHandler *errh)
{
	bool _enable;
	XIACidFilter *f = static_cast<XIACidFilter *>(e);

	if (cp_va_kparse(conf, f, errh,
					"ENABLE", cpkP + cpkM, cpBool, &_enable,
					cpEnd) < 0) {

		return -1;
	}

	f->enabled = _enable;
	return 0;
}

String XIACidFilter::status(Element *e, void * /*thunk*/)
{
	XIACidFilter* f = static_cast<XIACidFilter*>(e);

	return f->enabled ? "enabled\n" : "disabled\n";
}

void XIACidFilter::add_handlers()
{
	add_read_handler("status", status, 0);
	add_write_handler("enable", toggle, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIACidFilter)
ELEMENT_MT_SAFE(XIACidFilter)

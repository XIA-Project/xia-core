/*
 * XIASCIONForwarder.{cc,hh} -- Forwarding engine for the SCIONID XIA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>

#include <click/config.h>
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/xiaheader.hh>
#include <click/xiascionheader.hh>
#include "xiascionforwarder.hh"

CLICK_DECLS

// FIXME: this should probably be replaced with DESTINED_FOR_DISCARD
#define DESTINED_FOR_SCION_DISCARD -10

XIASCIONForwarder::XIASCIONForwarder()
{
}

XIASCIONForwarder::~XIASCIONForwarder()
{
	_rts.clear();
}

int XIASCIONForwarder::initialize(ErrorHandler *)
{
	// XLog installed the syslog error handler, use it!
	_errh = (SyslogErrorHandler*)ErrorHandler::default_handler();
	return 0;
}

int
XIASCIONForwarder::configure(Vector<String> &conf, ErrorHandler *errh)
{
//	DBG("XIASCIONForwarder: configuring %s\n", this->name().c_str());

	_principal_type_enabled = 1;
	_num_ports = 0;

	_rtdata.port = -1;
	_rtdata.flags = 0;
	_rtdata.nexthop = NULL;

	Element *elem;

	if (cp_va_kparse(conf, this, errh,
		"NUM_PORT", cpkP+cpkM, cpInteger, &_num_ports,
		"HID_TABLE", cpkP+cpkM, cpElement, &elem,
		cpEnd) < 0)
	return -1;

	_hid_table = (XIAXIDRouteTable*)elem;
	return 0;
}

int
XIASCIONForwarder::set_enabled(int e)
{
	_principal_type_enabled = e;
	return 0;
}

int XIASCIONForwarder::get_enabled()
{
	return _principal_type_enabled;
}

void
XIASCIONForwarder::add_handlers()
{
	add_write_handler("enabled", write_handler, (void *)PRINCIPAL_TYPE_ENABLED);
	add_read_handler("enabled", read_handler, (void *)PRINCIPAL_TYPE_ENABLED);

	// FIXME: is there any need to set any route info into the class
	// or is all we need a default route and scion & the HID table hack
	// do the rest?
	add_write_handler("add", set_handler, 0);
	add_write_handler("set", set_handler, (void*)1);
	add_write_handler("add4", set_handler4, 0);
	add_write_handler("set4", set_handler4, (void*)1);
	add_write_handler("remove", remove_handler, 0);
	add_read_handler("list", list_routes_handler, 0);
}

String
XIASCIONForwarder::read_handler(Element *e, void *thunk)
{
	XIASCIONForwarder *t = (XIASCIONForwarder *) e;
	switch ((intptr_t)thunk) {
		case PRINCIPAL_TYPE_ENABLED:
			return String(t->get_enabled());

		default:
			return "<error>";
	}
}

int
XIASCIONForwarder::write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh)
{
	XIASCIONForwarder *t = (XIASCIONForwarder *) e;
	switch ((intptr_t)thunk) {
		case PRINCIPAL_TYPE_ENABLED:
			return t->set_enabled(atoi(str.c_str()));

		default:
			return -1;
	}
}

String
XIASCIONForwarder::list_routes_handler(Element *e, void * /*thunk */)
{
	XIASCIONForwarder* table = static_cast<XIASCIONForwarder*>(e);
	XIARouteData *xrd = &table->_rtdata;

	// get the default route
	String tbl = "-," + String(xrd->port) + "," +
		(xrd->nexthop != NULL ? xrd->nexthop->unparse() : "") + "," +
		String(xrd->flags) + "\n";

	// get the rest
	HashTable<XID, XIARouteData *>::iterator it = table->_rts.begin();
	while (it != table->_rts.end()) {
		String xid = it.key().unparse();

		xrd = (XIARouteData *)it.value();

		tbl += xid + ",";
		tbl += String(xrd->port) + ",";
		tbl += (xrd->nexthop != NULL ? xrd->nexthop->unparse() : "") + ",";
		tbl += String(xrd->flags) + "\n";
		it++;
	}
	return tbl;
}

int
XIASCIONForwarder::set_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
	// handle older style route entries

	String str_copy = conf;
	String xid_str = cp_shift_spacevec(str_copy);

	if (xid_str.length() == 0)
	{
		// ignore empty entry
		return 0;
	}

	int port;
	if (!cp_integer(str_copy, &port))
		return errh->error("invalid port: ", str_copy.c_str());

	String str = xid_str + "," + String(port) + ",,0";

	return set_handler4(str, e, thunk, errh);
}

int
XIASCIONForwarder::set_handler4(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
	XIASCIONForwarder* table = static_cast<XIASCIONForwarder*>(e);

	bool add_mode = !thunk;

	Vector<String> args;
	int port = 0;
	unsigned flags = 0;
	String xid_str;
	XID *nexthop = NULL;

	cp_argvec(conf, args);

	if (args.size() < 2 || args.size() > 4)
		return errh->error("invalid route: ", conf.c_str());

	xid_str = args[0];

	if (!cp_integer(args[1], &port))
		return errh->error("invalid port: ", conf.c_str());

	if (args.size() == 4) {
		if (!cp_integer(args[3], &flags))
			return errh->error("invalid flags: ", conf.c_str());
	}

	if (args.size() >= 3 && args[2].length() > 0) {
		String nxthop = args[2];
		nexthop = new XID;
		cp_xid(nxthop, nexthop, e);
		//nexthop = new XID(args[2]);
		if (!nexthop->valid()) {
			delete nexthop;
			return errh->error("invalid next hop xid: ", conf.c_str());
		}
	}

	if (xid_str == "-") {
		if (add_mode && table->_rtdata.port != -1)
			return errh->error("duplicate default route: ", xid_str.c_str());
		table->_rtdata.port= port;
		table->_rtdata.flags = flags;
		table->_rtdata.nexthop = nexthop;
	} else {
		XID xid;
		if (!cp_xid(xid_str, &xid, e)) {
			if (nexthop) delete nexthop;
			return errh->error("invalid XID: ", xid_str.c_str());
		}
		if (add_mode && table->_rts.find(xid) != table->_rts.end()) {
			if (nexthop) delete nexthop;
			return errh->error("duplicate XID: ", xid_str.c_str());
		}

		XIARouteData *xrd = new XIARouteData();

		xrd->port = port;
		xrd->flags = flags;
		xrd->nexthop = nexthop;
		table->_rts[xid] = xrd;
	}

	return 0;
}

int
XIASCIONForwarder::remove_handler(const String &xid_str, Element *e, void *, ErrorHandler *errh)
{
	XIASCIONForwarder* table = static_cast<XIASCIONForwarder*>(e);

	if (xid_str.length() == 0)
	{
		// ignore empty entry
		return 0;
	}

	if (xid_str == "-") {
		table->_rtdata.port = -1;
		table->_rtdata.flags = 0;
		if (table->_rtdata.nexthop) {
			delete table->_rtdata.nexthop;
			table->_rtdata.nexthop = NULL;
		}

	} else {
		XID xid;
		if (!cp_xid(xid_str, &xid, e))
			return errh->error("invalid XID: ", xid_str.c_str());
		HashTable<XID, XIARouteData*>::iterator it = table->_rts.find(xid);
		if (it == table->_rts.end())
			return errh->error("nonexistent XID: ", xid_str.c_str());

		XIARouteData *xrd = (XIARouteData*)it.value();

		// FIXME: delete the nxthop xid if any
		table->_rts.erase(it);
		delete xrd;
	}
	return 0;
}

void
XIASCIONForwarder::push(int in_ether_port, Packet *p)
{
	int port;

	DBG("==== SCION FORWARD ENGINE: PUSH ====\n");
#if 0
	// noisy debug code
	XIAHeader xhdr(p);
	ScionHeader shdr(p);

	xhdr.dump();
	shdr.dump();
#endif

	in_ether_port = XIA_PAINT_ANNO(p);

	if (!_principal_type_enabled) {
		output(2).push(p);
		return;
	}

	port = lookup_route(in_ether_port, p);

	INFO(" SCION output port = %d\n", port);

	if (port >= 0) {
		SET_XIA_PAINT_ANNO(p,port);
		output(0).push(p);
	}
	else if (port == DESTINED_FOR_LOCALHOST) {
		output(1).push(p);
	}
	else {
		output(2).push(p);
	}
}


int
XIASCIONForwarder::lookup_route(int in_ether_port, Packet *p)
{
	const struct click_xia* hdr = p->xia_header();
	int last = hdr->last;

	if (last < 0)
		last += hdr->dnode;

//	DBG("SCION route lookup");
//	DBG("scion: last pointer %d\n", last);

	// FIXME: does this block need to change or go away for SCION?
	const struct click_xia_xid_edge* edge = hdr->node[last].edge;
	const struct click_xia_xid_edge& current_edge = edge[XIA_NEXT_PATH_ANNO(p)];
	const int& idx = current_edge.idx;
	if (idx == CLICK_XIA_XID_EDGE_UNUSED)
	{
		// unused edge -- use default route
		INFO("WE GET HERE, CODE IS STILL NEEDED");
		return _rtdata.port;
	}

	const struct click_xia_xid_node& node = hdr->node[idx];
	XIAHeader xiah(p->xia_header());

	//scion info is in scion ext header
	int port = scion_forward_packet(hdr);

	INFO("SCION OUTPUT PORT=%d\n", port);

	// FIXME:
	// a) this should probably just be DESTINED_FOR_DISCARD
	// b) this will break if DESTINED_FOR_SCION_DISCARD < port < 0
	if (port > DESTINED_FOR_SCION_DISCARD) {
		XID nexthop;
		_hid_table->next_hop(port, nexthop);
		INFO("nexthop = %s\n", nexthop.unparse().c_str());
		p->set_nexthop_neighbor_xid_anno(nexthop);

	} else {
		port = DESTINED_FOR_DISCARD;
	}

	return port;
}

void XIASCIONForwarder::print_packet_contents(uint8_t *packet, int len)
{
	int hex_string_len = (len*2) + 1;
	char hex_string[hex_string_len];
	int i;
	uint8_t* data = (uint8_t*)packet;
	bzero(hex_string, hex_string_len);
	for(i=0;i<len;i++) {
		sprintf(&hex_string[2*i], "%02x", (unsigned int)data[i]);
	}
	hex_string[hex_string_len-1] = '\0';
	INFO("Packet contents|%s|", hex_string);
}


uint16_t XIASCIONForwarder::hof_get_ingress(HopOpaqueField *hof)
{
	return ((uint16_t)hof->ingress_egress[0]) << 4 | ((uint16_t)hof->ingress_egress[1] & 0xf0) >> 4;
}

uint16_t XIASCIONForwarder::hof_get_egress(HopOpaqueField *hof)
{
	return ((uint16_t)hof->ingress_egress[1] & 0xf) << 8 | ((uint16_t)hof->ingress_egress[2]);
}

uint16_t XIASCIONForwarder::iof_get_isd(InfoOpaqueField* iof)
{
	return (uint16_t)iof->isd_id[0] << 8 | iof->isd_id[1];
}

uint32_t XIASCIONForwarder::iof_get_timestamp(InfoOpaqueField* iof)
{
	return ((uint32_t)iof->timestamp[0] << 24) | ((uint32_t)iof->timestamp[2] << 16) | ((uint32_t)iof->timestamp[1] << 8) | (uint32_t)iof->timestamp[0];
}

uint8_t XIASCIONForwarder::is_on_up_path(InfoOpaqueField *currIOF)
{
	// low bit of type field is used for uppath/downpath flag
	if ((currIOF->info & 0x1) == 1) {
		INFO("is on up path\n");
		return 1;
	}
	return 0;
}

bool XIASCIONForwarder::is_last_path_of(SCIONCommonHeader *sch)
{
	uint8_t offset = sch->headerLen -	sizeof(HopOpaqueField);
	INFO("is_last_path_of %d %d\n",sch->currentOF, offset);
	return sch->currentOF == offset;
}

int XIASCIONForwarder::print_scion_path_info(uint8_t* path, uint32_t path_len)
{
	InfoOpaqueField *iof = (InfoOpaqueField *)path;
	INFO("Print scion path info, path length %d bytes:\n", path_len);
	INFO("InfoOpaqueField:\n");
	INFO("info %x\n", iof->info >> 1);
	INFO("flag %x\n", iof->info & 0x1);

	print_packet_contents((uint8_t*)iof, 8);
	INFO("info %#x, flag %d, isd-id %d, hops %d\n", iof->info >> 1, iof->info & 0x1, iof_get_isd(iof), iof->hops);

	for(int i = 0; i < 2; i++) {
		HopOpaqueField *hof = (HopOpaqueField *)((uint8_t *)path + sizeof(InfoOpaqueField) + i * sizeof(HopOpaqueField));
		print_packet_contents((uint8_t*)hof, sizeof(HopOpaqueField));
		INFO("Ingress %d, Egress %d\n", hof_get_ingress(hof), hof_get_egress(hof));
	}
	return 0;
}

int XIASCIONForwarder::print_scion_header(uint8_t *hdr)
{
	SCIONCommonHeader *sch = (SCIONCommonHeader *)hdr;
	HopOpaqueField *hof;
	InfoOpaqueField *iof;
	INFO("print scion common header:");

	INFO("versionAddrs : %d\n", ntohs(sch->versionSrcDst));
	INFO("totalLen: %d\n", ntohs(sch->totalLen));
	INFO("currentIOF: %d\n", sch->currentIOF);
	INFO("currentOF: %d\n", sch->currentOF);
	INFO("nextHeader: %d\n", sch->nextHeader);
	INFO("headerLen: %d\n", sch->headerLen);

	iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);
	print_packet_contents((uint8_t*)iof, 8);
	INFO("info %#x, flag %d, isd-id %d, hops %d\n", iof->info >> 1, iof->info & 0x1, iof_get_isd(iof), iof->hops);

	for(int i = 0; i < 2; i++) {
		hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentIOF + sizeof(InfoOpaqueField) + i * sizeof(HopOpaqueField));
		print_packet_contents((uint8_t*)hof, sizeof(HopOpaqueField));
		INFO("info is %#x, Ingress %d, Egress %d\n", hof->info, hof_get_ingress(hof), hof_get_egress(hof));
	}

	return 0;
}

int XIASCIONForwarder::handle_ingress_xovr(SCIONCommonHeader *sch)
{
	HopOpaqueField *hof;
	InfoOpaqueField *iof;

	INFO("handle ingress xovr\n");
	hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF);
	iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);
	INFO("iof->info %d\n", iof->info);

	if (iof->info >>1 == IOF_SHORTCUT) {
		return ingress_shortcut_xovr(sch);
	} else if (iof->info >>1 == IOF_INTRA_ISD_PEER || iof->info >>1 == IOF_INTER_ISD_PEER) {
		return ingress_peer_xovr(sch);
	} else if (iof->info >>1 == IOF_CORE) {
		return ingress_core_xovr(sch);
	} else {
		INFO("Invalid iof->info %d\n", iof->info);
		return DESTINED_FOR_SCION_DISCARD;
	}
	return DESTINED_FOR_SCION_DISCARD;
}

int XIASCIONForwarder::ingress_shortcut_xovr(SCIONCommonHeader *sch)
{
	HopOpaqueField *hof;
	HopOpaqueField *prev_hof;
	InfoOpaqueField *iof;

	INFO("ingress shortcut xovr\n");
	hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF);
	iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);

	prev_hof = hof + 1;
	if (verify_of(hof, prev_hof, iof_get_timestamp(iof)) == 0) {
		return DESTINED_FOR_SCION_DISCARD;
	}

	// switch to next segment
	sch->currentIOF = sch->currentOF + sizeof(HopOpaqueField) * 2;
	sch->currentOF += sizeof(HopOpaqueField) * 4;

	hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF);
	iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);

	if (hof_get_ingress(hof) == 0 && is_last_path_of(sch)) {
		prev_hof = hof - 1;
		if (verify_of(hof, prev_hof, iof_get_timestamp(iof)) == 0) {
			return DESTINED_FOR_SCION_DISCARD;
		}
		//deliver(m, DATA_PACKET, dpdk_rx_port);
		return DESTINED_FOR_LOCALHOST;
	} else {
		return hof_get_egress(hof);
		//send_ingress(m, EGRESS_IF(hof), dpdk_rx_port);
	}
}

int XIASCIONForwarder::ingress_peer_xovr(SCIONCommonHeader *sch)
{
	HopOpaqueField *hof;
	HopOpaqueField *prev_hof;
	InfoOpaqueField *iof;

	INFO("ingress peer xovr\n");
	hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF);
	iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);

	uint16_t fwd_if;
	if (is_on_up_path(iof)) {
		prev_hof = hof + 2; // why + 2?
		fwd_if = hof_get_ingress(hof + 1);
	} else {
		prev_hof = hof + 1; // why + 1?
		fwd_if = hof_get_egress(hof + 1);
	}

	if (verify_of(hof, prev_hof, iof_get_timestamp(iof)) == 0) {
		return DESTINED_FOR_SCION_DISCARD;
	}

	sch->currentOF += sizeof(HopOpaqueField);

	if (is_last_path_of(sch))
		//deliver(m, DATA_PACKET, dpdk_rx_port);
		return DESTINED_FOR_LOCALHOST;
	else
		//send_ingress(m, fwd_if, dpdk_rx_port);
		return fwd_if;
}

int XIASCIONForwarder::ingress_core_xovr(SCIONCommonHeader *sch)
{
	HopOpaqueField *hof;
	HopOpaqueField *prev_hof;
	InfoOpaqueField *iof;

	INFO("ingress core xovr\n");
	hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF);
	iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);

	uint32_t fwd_if;
	if (is_on_up_path(iof)) {
		prev_hof = NULL;
	} else {
		prev_hof = hof - 1;
	}

	if (verify_of(hof, prev_hof, iof_get_timestamp(iof)) == 0) {
		return DESTINED_FOR_SCION_DISCARD;
	}

	if (is_last_path_of(sch)) {
		INFO("is the last path\n");
		return DESTINED_FOR_LOCALHOST;
	} else {
		// Switch to next path segment
		INFO("switch to next segment\n");
		sch->currentIOF = sch->currentOF + sizeof(HopOpaqueField);
		sch->currentOF += sizeof(HopOpaqueField) * 2;
		iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);
		hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF);

		if (is_on_up_path(iof)) {
			//send_ingress(m, INGRESS_IF(hof), dpdk_rx_port);
			return hof_get_ingress(hof);
		} else {
			//send_ingress(m, EGRESS_IF(hof), dpdk_rx_port);
			return hof_get_egress(hof);
		}
	}

	return DESTINED_FOR_SCION_DISCARD;
}

int XIASCIONForwarder::ingress_normal_forward(SCIONCommonHeader *sch)
{
	HopOpaqueField *hof;
	HopOpaqueField *prev_hof;
	InfoOpaqueField *iof;

	INFO("ingress normal forward\n");
	hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF);
	iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);

	INFO("Ingress %d, Egress %d\n", hof_get_ingress(hof), hof_get_egress(hof));
	uint16_t next_ifid;
	if (is_on_up_path(iof)) {
		next_ifid = hof_get_ingress(hof);
		prev_hof = hof + 1;
	} else {
		next_ifid = hof_get_egress(hof);
		prev_hof = hof - 1;
	}

	INFO("Next ifid %d\n", next_ifid);

	if (verify_of(hof, prev_hof, iof_get_timestamp(iof)) == 0) {
		return DESTINED_FOR_SCION_DISCARD;
	}

	if (next_ifid == 0 && is_last_path_of(sch)) {
		//deliver(m, DATA_PACKET, dpdk_rx_port);
		return DESTINED_FOR_LOCALHOST;
	} else {
		//send_ingress(m, next_ifid, dpdk_rx_port);
		return next_ifid;
	}

	return DESTINED_FOR_SCION_DISCARD;
}

int XIASCIONForwarder::handle_egress_xovr(SCIONCommonHeader *sch)
{
	HopOpaqueField *hof;
	InfoOpaqueField *iof;

	INFO("handle egress xovr\n");
	iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);

	if (iof->info >>1 == IOF_SHORTCUT) {
		return egress_shortcut_xovr(sch);
	} else if (iof->info >>1 == IOF_INTRA_ISD_PEER ||
						iof->info	>>1== IOF_INTER_ISD_PEER) {
		return egress_peer_xovr(sch);
	} else if (iof->info	>>1== IOF_CORE) {
		return egress_core_xovr(sch);
	} else {
		// invalid OF
		INFO("Invalid iof->info %#x\n", iof->info);
	}
	return DESTINED_FOR_SCION_DISCARD;
}

int XIASCIONForwarder::egress_shortcut_xovr(SCIONCommonHeader *sch)
{
	INFO("egress_shortcut_xovr\n");
	return egress_normal_forward(sch);
}

int XIASCIONForwarder::egress_peer_xovr(SCIONCommonHeader *sch)
{
	HopOpaqueField *hof;
	HopOpaqueField *prev_hof;
	InfoOpaqueField *iof;

	INFO("egress_peer_xovr \n");
	hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF);
	iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);

	if (is_on_up_path(iof)) {
		prev_hof = hof - 1;
		if (verify_of(hof, prev_hof, iof_get_timestamp(iof)) == 0) {
			return DESTINED_FOR_SCION_DISCARD;
		}
		// Switch to next segment
		sch->currentIOF = sch->currentOF + sizeof(HopOpaqueField) * 2;
		sch->currentOF += sizeof(HopOpaqueField) * 4; // why *4?
	} else {
		prev_hof = hof - 2;
		if (verify_of(hof, prev_hof, iof_get_timestamp(iof)) == 0) {
			return DESTINED_FOR_SCION_DISCARD;
		}
		sch->currentOF += sizeof(HopOpaqueField); // why not *4?
	}
	// now we do not support send_egress
	//send_egress(m, dpdk_rx_port);
	return DESTINED_FOR_SCION_DISCARD;
}

int XIASCIONForwarder::egress_core_xovr(SCIONCommonHeader *sch)
{
	HopOpaqueField *hof;
	HopOpaqueField *prev_hof;
	InfoOpaqueField *iof;

	INFO("egress core xovr\n");
	hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF);
	iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);

	if (is_on_up_path(iof)) {
		prev_hof = NULL;
	} else {
		prev_hof = hof + 1;
	}

	if (verify_of(hof, prev_hof, iof_get_timestamp(iof)) == 0) {
		return DESTINED_FOR_SCION_DISCARD;
	}

	sch->currentOF += sizeof(HopOpaqueField);

	//send_egress send a packet to a neighbor AD router? check it
	//send_egress(m, dpdk_rx_port);
	return DESTINED_FOR_SCION_DISCARD;
}

int XIASCIONForwarder::egress_normal_forward(SCIONCommonHeader *sch)
{
	HopOpaqueField *hof;
	HopOpaqueField *prev_hof;
	InfoOpaqueField *iof;

	INFO("egress normal forward\n");
	hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF);
	iof = (InfoOpaqueField *)((unsigned char *)sch + sch->currentIOF);

	if (is_on_up_path(iof)) {
		prev_hof = hof + 1;
	} else {
		prev_hof = hof - 1;
	}

	if (verify_of(hof, prev_hof, iof_get_timestamp(iof)) == 0) {
		return DESTINED_FOR_SCION_DISCARD;
	}

	sch->currentOF += sizeof(HopOpaqueField);

	// send packet to neighbor AD's router
	//send_egress(m, dpdk_rx_port);
	return DESTINED_FOR_SCION_DISCARD;
}

//
//skip verification now as the routers do not have mac key
//
int XIASCIONForwarder::verify_of(HopOpaqueField *hof, HopOpaqueField *prev_hof, uint32_t ts)
{
	return 1;
}

int XIASCIONForwarder::scion_forward_packet(const struct click_xia* xiah)
{
	uint8_t* packet = (uint8_t *)(xiah);
	int total_nodes = xiah->dnode + xiah->snode;
	uint16_t xia_hdr_len =	sizeof(struct click_xia) + total_nodes*sizeof(struct click_xia_xid_node);
	uint16_t hdr_len = xia_hdr_len;
	struct click_xia_ext* xia_ext_hdr = (struct click_xia_ext *)(packet + hdr_len);
	uint8_t nxt_hdr_type = xiah->nxt;

	while((nxt_hdr_type != CLICK_XIA_NXT_SCION) && (nxt_hdr_type != CLICK_XIA_NXT_NO)) {
		INFO("next header type %d, xia hdr len %d ", (int)nxt_hdr_type, hdr_len);
		nxt_hdr_type = xia_ext_hdr->nxt;
		hdr_len += xia_ext_hdr->hlen;
		xia_ext_hdr = (struct click_xia_ext *)(packet + hdr_len);
	}

	if(nxt_hdr_type == CLICK_XIA_NXT_SCION) {
		print_packet_contents((uint8_t*)xia_ext_hdr, xia_ext_hdr->hlen);
		
		SCIONCommonHeader *sch = (SCIONCommonHeader*)((uint8_t*)xia_ext_hdr + 2);
		print_scion_header((uint8_t*)sch);
		
		uint8_t srcLen = SCION_ISD_AD_LEN + SCION_ADDR_LEN;
		uint8_t dstLen = SCION_ISD_AD_LEN + SCION_ADDR_LEN;
		uint8_t *path = (uint8_t*)sch + sizeof(SCIONCommonHeader) + srcLen + dstLen;
		uint8_t path_length = sch->headerLen - sizeof(SCIONCommonHeader) - srcLen - dstLen;

		print_scion_path_info(path, path_length);

		InfoOpaqueField* iof = (InfoOpaqueField*)((uint8_t*)sch + sch->currentIOF);

		HopOpaqueField *hof = (HopOpaqueField *)((unsigned char *)sch + sch->currentOF);

		INFO("forward_packet: hof->info=%#x, currentIOF %d, currentOF %d\n", hof->info, sch->currentIOF, sch->currentOF);
		INFO("egress interface %d", hof_get_egress(hof));

#if 1
		// in XIA-SCION, we do not support local_ad/socket now,
		// so we only use egress interface for routing now.
		// todo: support from_local_ad
		//if(!is_last_path_of(sch)) {
		if(sch->currentOF < sch->headerLen) {
			int egress_interface = hof_get_egress(hof);
			sch->currentOF += sizeof(HopOpaqueField);
			INFO("return_egress interface %d", egress_interface);
			return egress_interface;
		} else {
			return DESTINED_FOR_LOCALHOST;
	}
#else
		// now per router per AD, so never from local socket/AD
		uint8_t from_local_ad = 1;
		if (from_local_ad == 0) {
			if (hof->info == XOVR_POINT) {
				return handle_ingress_xovr(sch);
			} else {
				return ingress_normal_forward(sch);
			}
		} else {
			if (hof->info == XOVR_POINT) {
				return handle_egress_xovr(sch);
			} else {
				return egress_normal_forward(sch);
			}
		}
#endif
	} else {
		INFO("no SCION header!! return DESTINED_FOR_SCION_DISCARD");
		return DESTINED_FOR_SCION_DISCARD; //unused for SCION now
	}

	return DESTINED_FOR_LOCALHOST;
}


CLICK_ENDDECLS
EXPORT_ELEMENT(XIASCIONForwarder)
ELEMENT_MT_SAFE(XIASCIONForwarder)

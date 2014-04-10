/*
 * xiaxidmultiroutetable.{cc,hh} -- simple XID routing table with multiple entries per XID
 */

#include <click/config.h>
#include "xiaxidmultiroutetable.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/xiaheader.hh>
#if CLICK_USERLEVEL
#include <fstream>
#include <stdlib.h>
#endif
CLICK_DECLS

XIAXIDMultiRouteTable::XIAXIDMultiRouteTable(): _drops(0)
{
}

XIAXIDMultiRouteTable::~XIAXIDMultiRouteTable()
{
    _rts.clear();
}

int
XIAXIDMultiRouteTable::configure(Vector<String> &conf, ErrorHandler *errh)
{
    click_chatter("XIAXIDMultiRouteTable: configuring %s\n", this->name().c_str());

    _principal_type_enabled = 1;
    _num_ports = 0;

    _rtdata.port = -1;
    _rtdata.flags = 0;
    _rtdata.nexthop = NULL;

    XIAPath local_addr;

    if (cp_va_kparse(conf, this, errh,
        "LOCAL_ADDR", cpkP+cpkM, cpXIAPath, &local_addr,
        "NUM_PORT", cpkP+cpkM, cpInteger, &_num_ports,
        cpEnd) < 0)
    return -1;

    _local_addr = local_addr;
    _local_hid = local_addr.xid(local_addr.destination_node());

    String broadcast_xid(BHID);  // broadcast HID
    _bcast_xid.parse(broadcast_xid);

    return 0;
}

int
XIAXIDMultiRouteTable::set_enabled(int e)
{
    _principal_type_enabled = e;
    return 0;
}

int XIAXIDMultiRouteTable::get_enabled()
{
    return _principal_type_enabled;
}

void
XIAXIDMultiRouteTable::add_handlers()
{
    //click_chatter("XIAXIDMultiRouteTable:add_hand");
    add_write_handler("add", set_handler, 0);
    add_write_handler("set", set_handler, (void*)1);
    add_write_handler("add4", set_handler4, 0);
    add_write_handler("set4", set_handler4, (void*)1);
    add_write_handler("app4", set_handler4, (void*)2);
    add_write_handler("sel_set4", set_handler4, (void*)3); // bad name?
    add_write_handler("remove", remove_handler, 0);
    add_write_handler("load", load_routes_handler, 0);
    add_write_handler("generate", generate_routes_handler, 0);
    add_data_handlers("drops", Handler::OP_READ, &_drops);
    add_read_handler("list", list_routes_handler, 0);
    add_write_handler("enabled", write_handler, (void *)PRINCIPAL_TYPE_ENABLED);
    add_read_handler("enabled", read_handler, (void *)PRINCIPAL_TYPE_ENABLED);
}

String
XIAXIDMultiRouteTable::read_handler(Element *e, void *thunk)
{
    //click_chatter("XIAXIDMultiRouteTable:read_hand");
    XIAXIDMultiRouteTable *t = (XIAXIDMultiRouteTable *) e;
    switch ((intptr_t)thunk) {
        case PRINCIPAL_TYPE_ENABLED:
            return String(t->get_enabled());

        default:
            return "<error>";
    }
}

int
XIAXIDMultiRouteTable::write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh)
{
    //click_chatter("XIAXIDMultiRouteTable:write_hand");
    XIAXIDMultiRouteTable *t = (XIAXIDMultiRouteTable *) e;
    switch ((intptr_t)thunk) {
        case PRINCIPAL_TYPE_ENABLED:
            return t->set_enabled(atoi(str.c_str()));

        default:
            return -1;
    }
}

String
XIAXIDMultiRouteTable::list_routes_handler(Element *e, void * /*thunk */)
{
    //click_chatter("XIAXIDMultiRouteTable:list_routes_handler");
    XIAXIDMultiRouteTable* table = static_cast<XIAXIDMultiRouteTable*>(e);
    XIAMultiRouteData *xrd = &table->_rtdata;

    // get the default route
    String tbl = "-," + String(xrd->port) + "," +
        (xrd->nexthop != NULL ? xrd->nexthop->unparse() : "") + "," +
        String(xrd->flags) + "\n";

    // get the rest
    HashTable<XID, Vector<XIAMultiRouteData*> >::iterator it = table->_rts.begin();
    while (it != table->_rts.end()) {
        String xid = it.key().unparse();
        Vector<XIAMultiRouteData*>::iterator it_entry = it.value().begin();
        while (it_entry != it.value().end())
        {
            xrd = *it_entry;
            tbl += xid + ",";
            tbl += String(xrd->port) + ",";
            tbl += (xrd->nexthop != NULL ? xrd->nexthop->unparse() : "") + ",";
            tbl += String(xrd->flags) + "\n";
            // TODO: change xia route print to allow extra outputs
            //tbl += String(xrd->weight) + "\n";
            ++it_entry;
        }
        ++it;
    }
    return tbl;
}

int
XIAXIDMultiRouteTable::set_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
    // handle older style route entries
    //click_chatter("XIAXIDMultiRouteTable:set_handler");
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
XIAXIDMultiRouteTable::set_handler4(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
    //click_chatter("XIAXIDMultiRouteTable:set_handler4");
    XIAXIDMultiRouteTable* table = static_cast<XIAXIDMultiRouteTable*>(e);

    int mode = *((int*)(&thunk));//ugly cast 0:add 1:set 2:append 3: selective update
    bool add_mode = (mode == 0);
    bool set_mode = (mode == 1);
    bool app_mode = (mode == 2);
    bool update_mode = (mode == 3);

    Vector<String> args;
    int port = 0;
    unsigned flags = 0;
    unsigned weight = 100; // default weight use 100%
    String xid_str, index;
    XID *nexthop = NULL;

    cp_argvec(conf, args);

    if (args.size() < 2 || args.size() > 6)
        return errh->error("invalid route: ", conf.c_str());

    xid_str = args[0];

    if (!cp_integer(args[1], &port))
        return errh->error("invalid port: ", conf.c_str());

    if (args.size() >= 4) {
        if (!cp_integer(args[3], &flags))
            return errh->error("invalid flags: ", conf.c_str());
    }

    if (args.size() >= 5) {
        if (!cp_integer(args[4], &weight))
            return errh->error("invalid weight: ", conf.c_str());
    }

    if (args.size() == 6){
        index = args[5];
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
        table->_rtdata.weight = weight; // no use
        table->_rtdata.index = index; // no use
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
        if (table->_rts.find(xid) == table->_rts.end()) { // the Vector is not there
            Vector<XIAMultiRouteData*> list;
            table->_rts[xid] = list;
        }
        XIAMultiRouteData *xrd = NULL;
        if (add_mode || app_mode){//both mode add one entry to the vector TODO: app mode should refuse to add entry with existing index
            xrd = new XIAMultiRouteData();
            table->_rts[xid].push_back(xrd);
        }
        else if (set_mode){//change or add if no exist
            if ( table->_rts[xid].size() == 0 ){
                xrd = new XIAMultiRouteData();
                table->_rts[xid].push_back(xrd);
            }
            xrd = table->_rts[xid][0];
        }
        else if (update_mode){ // change a entry with a specific index
            Vector<XIAMultiRouteData*>::iterator it_entry = table->_rts[xid].begin();
            while ( it_entry != table->_rts[xid].end()){// linear search, TODO: hashmap<index, Entry>?
                if ((*it_entry)-> index == index ){
                    break;
                }
                ++it_entry;
            }
            if (it_entry == table->_rts[xid].end()){// not found
                xrd = new XIAMultiRouteData();
                table->_rts[xid].push_back(xrd);
            }
            else{
                xrd = *(it_entry);
            }

        }
        xrd->port = port;
        xrd->flags = flags;
        xrd->nexthop = nexthop;
        xrd->weight = weight;
        xrd->index = index;

        // TODO: should find the same entry (same port ?) and override the values
        // TODO: add index (AD?) into XIAMultiRouteData to identify different routes
    }

    return 0;
}

int
XIAXIDMultiRouteTable::remove_handler(const String &xid_str, Element *e, void *, ErrorHandler *errh)
{
    //TODO: seletive delete
    //click_chatter("XIAXIDMultiRouteTable:remove_handler");
    XIAXIDMultiRouteTable* table = static_cast<XIAXIDMultiRouteTable*>(e);

    if (xid_str.length() == 0)
    {
        // ignore empty entry
        return 0;
    }

    if (xid_str == "-") {
        table->_rtdata.port = -1;
        table->_rtdata.flags = 0; //NOTE: Should I change weight for defalut route?
        if (table->_rtdata.nexthop) {
            delete table->_rtdata.nexthop;
            table->_rtdata.nexthop = NULL;
        }

    } else {
        XID xid;
        if (!cp_xid(xid_str, &xid, e))
            return errh->error("invalid XID: ", xid_str.c_str());
        HashTable<XID, Vector<XIAMultiRouteData*> >::iterator it = table->_rts.find(xid);
        if (it == table->_rts.end())
            return errh->error("nonexistent XID: ", xid_str.c_str());
        // TODO: ability to delete one entry besides delete the whole vector
        Vector<XIAMultiRouteData*>::iterator it_entry = it.value().begin();
        while (it_entry != it.value().end() ){
            XIAMultiRouteData *xrd = *it_entry;
            if (xrd->nexthop){
                delete xrd->nexthop;
                (*it_entry)->nexthop = NULL;
            }
            delete xrd;
            ++it_entry;
        }
        table->_rts.erase(it);
        // remove the whole list, NOTE: vector cannot be changed during the iteration
        // Or delete the key,value pair? which is better?
        Vector<XIAMultiRouteData*> list;
        table->_rts[xid] = list;
    }
    return 0;
}

int
XIAXIDMultiRouteTable::load_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
    //click_chatter("XIAXIDMultiRouteTable:load_routes_handler");
#if CLICK_USERLEVEL
    std::ifstream in_f(conf.c_str());
    if (!in_f.is_open())
    {
        errh->error("could not open file: %s", conf.c_str());
        return -1;
    }

    int c = 0;
    while (!in_f.eof())
    {
        char buf[1024];
        in_f.getline(buf, sizeof(buf));

        if (strlen(buf) == 0)
            continue;

        if (set_handler(buf, e, 0, errh) != 0)
            return -1;

        c++;
    }
    click_chatter("loaded %d entries", c);

    return 0;
#elif CLICK_LINUXMODLE
    int c = 0;
    char buf[1024];

    mm_segment_t old_fs = get_fs();
    set_fs(KERNEL_DS);
    struct file * filp = file_open(conf.c_str(), O_RDONLY, 0);
    if (filp==NULL)
    {
        errh->error("could not open file: %s", conf.c_str());
        return -1;
    }
    loff_t file_size = vfs_llseek(filp, (loff_t)0, SEEK_END);
    loff_t curpos = 0;
    while (curpos < file_size)  {
    file_read(filp, curpos, buf, 1020);
    char * eol = strchr(buf, '\n');
    if (eol==NULL) {
            click_chatter("Error at %s %d\n", __FUNCTION__, __LINE__);
        break;
    }
    curpos+=(eol+1-buf);
        eol[1] = '\0';
        if (strlen(buf) == 0)
            continue;

        if (set_handler(buf, e, 0, errh) != 0) {
            click_chatter("Error at %s %d\n", __FUNCTION__, __LINE__);
            return -1;
    }
        c++;
    }
    set_fs(old_fs);

    click_chatter("XIA routing table loaded %d entries", c);
    return 0;
#endif
}

int
XIAXIDMultiRouteTable::generate_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
    //click_chatter("XIAXIDMultiRouteTable:generate_routes_handler");
#if CLICK_USERLEVEL
    XIAXIDMultiRouteTable* table = dynamic_cast<XIAXIDMultiRouteTable*>(e);
#else
    XIAXIDMultiRouteTable* table = reinterpret_cast<XIAXIDMultiRouteTable*>(e);
#endif
    assert(table);

    String conf_copy = conf;

    String xid_type_str = cp_shift_spacevec(conf_copy);
    uint32_t xid_type;
    if (!cp_xid_type(xid_type_str, &xid_type))
        return errh->error("invalid XID type: ", xid_type_str.c_str());

    String count_str = cp_shift_spacevec(conf_copy);
    int count;
    if (!cp_integer(count_str, &count))
        return errh->error("invalid entry count: ", count_str.c_str());

    String port_str = cp_shift_spacevec(conf_copy);
    int port;
    if (!cp_integer(port_str, &port))
        return errh->error("invalid port: ", port_str.c_str());

#if CLICK_USERLEVEL
    unsigned short xsubi[3];
    xsubi[0] = 1;
    xsubi[1] = 2;
    xsubi[2] = 3;
//  unsigned short xsubi_next[3];
#else
    struct rnd_state state;
    prandom32_seed(&state, 1239);
#endif

    struct click_xia_xid xid_d;
    xid_d.type = xid_type;

    if (port<0) click_chatter("Random %d ports", -port);

    for (int i = 0; i < count; i++)
    {
        uint8_t* xid = xid_d.id;
        const uint8_t* xid_end = xid + CLICK_XIA_XID_ID_LEN;
#define PURE_RANDOM
#ifdef PURE_RANDOM
        uint32_t seed = i;
        memcpy(&xsubi[1], &seed, 2);
        memcpy(&xsubi[2], &(reinterpret_cast<char *>(&seed)[2]), 2);
        xsubi[0]= xsubi[2]+ xsubi[1];
#endif

        while (xid != xid_end)
        {
#if CLICK_USERLEVEL
#ifdef PURE_RANDOM
            *reinterpret_cast<uint32_t*>(xid) = static_cast<uint32_t>(nrand48(xsubi));
#else
            *reinterpret_cast<uint32_t*>(xid) = static_cast<uint32_t>(nrand48(xsubi));
#endif
#else
            *reinterpret_cast<uint32_t*>(xid) = static_cast<uint32_t>(prandom32(&state));
            if (i%5000==0)
                click_chatter("random value %x", *reinterpret_cast<uint32_t*>(xid));
#endif
            xid += sizeof(uint32_t);
        }

        /* random generation from 0 to |port|-1 */
        XIAMultiRouteData *xrd = new XIAMultiRouteData();
        xrd->flags = 0;
        xrd->nexthop = NULL;
        xrd->weight = 100;

        if (port<0) {
#if CLICK_LINUXMODULE
            u32 random = random32();
#else
            int random = rand();
#endif
            random = random % (-port);
            xrd->port = random;
            if (i%5000 == 0)
                click_chatter("Random port for XID %s #%d: %d ",XID(xid_d).unparse_pretty(e).c_str(), i, random);
        } else
            xrd->port = port;
        // how to do this? add a new entry(current approach) or replace the vector by a new vector with only one entry?
        HashTable<XID, Vector<XIAMultiRouteData*> >::iterator it = table->_rts.find(XID(xid_d));
        if (it == table->_rts.end()){// if not there, add a new vector
            Vector<XIAMultiRouteData*> list;
            table->_rts[XID(xid_d)] = list;
        }
        table->_rts[XID(xid_d)].push_back(xrd);
    }

    click_chatter("generated %d entries", count);
    return 0;
}


void
XIAXIDMultiRouteTable::push(int in_ether_port, Packet *p)
{
    //click_chatter("XIAXIDMultiRouteTable:push");

    int port;

    in_ether_port = XIA_PAINT_ANNO(p);

    if (!_principal_type_enabled) {
        output(2).push(p);
        return;
    }

    if(in_ether_port == REDIRECT) {
        // if this is an XCMP redirect packet
        process_xcmp_redirect(p);
        p->kill();
        return;
    } else {
        port = lookup_route(in_ether_port, p);
    }

    if(port == in_ether_port && in_ether_port !=DESTINED_FOR_LOCALHOST && in_ether_port !=DESTINED_FOR_DISCARD) { // need to inform XCMP that this is a redirect
      // "local" and "discard" shouldn't send a redirect
      Packet *q = p->clone();
      SET_XIA_PAINT_ANNO(q, (XIA_PAINT_ANNO(q)+TOTAL_SPECIAL_CASES)*-1);
      output(4).push(q);
    }
    if (port >= 0) {
      SET_XIA_PAINT_ANNO(p,port);
      output(0).push(p);
    }
    else if (port == DESTINED_FOR_LOCALHOST) {
      output(1).push(p);
    }
    else if (port == DESTINED_FOR_DHCP) {
      SET_XIA_PAINT_ANNO(p,port);
      output(3).push(p);
    }
    else if (port == DESTINED_FOR_BROADCAST) {
      for(int i = 0; i <= _num_ports; i++) {
        Packet *q = p->clone();
        SET_XIA_PAINT_ANNO(q,i);
        //q->set_anno_u8(PAINT_ANNO_OFFSET,i);
        output(0).push(q);
      }
      p->kill();
    }
    else {
      //SET_XIA_PAINT_ANNO(p,UNREACHABLE);

      //p->set_anno_u8(PAINT_ANNO_OFFSET,UNREACHABLE);

        // no match -- discard packet
      // Output 9 is for dropping packets.
      // let the routing engine handle the dropping.
      //_drops++;
      //if (_drops == 1)
      //      click_chatter("Dropping a packet with no match (last message)\n");
      //  p->kill();
      output(2).push(p);
    }
}

int
XIAXIDMultiRouteTable::process_xcmp_redirect(Packet *p)
{
    //click_chatter("XIAXIDMultiRouteTable:process_xcmp_redirect");

    // disable icmp redirect for table with multiple entries for one XID
    // redirect causes problems 1. fall to load balance 2. nexthop is redirect to a far-away router (unknown reason)

    /*
    XIAHeader hdr(p->xia_header());
    const uint8_t *pay = hdr.payload();
    XID *dest, *newroute;
    dest = new XID((const struct click_xia_xid &)(pay[4]));
    newroute = new XID((const struct click_xia_xid &)(pay[4+sizeof(struct click_xia_xid)]));

    // route update (dst, out, newroute, )
    HashTable<XID, Vector<XIAMultiRouteData*> >::const_iterator it = _rts.find(*dest);
    if (it != _rts.end() && it->second.size() == 1) {// single entry routes
        if ((*it).second[0]->nexthop){
            delete (*it).second[0]->nexthop;
        }
        (*it).second[0]->nexthop = newroute;
    }
    else if (it->second.size() > 1){// no rebind for multiple entries to make sure load balancing works
        delete newroute;
    }
    else {
        // Make a new entry for this XID
        Vector<XIAMultiRouteData*> list;
        XIAMultiRouteData *xrd1 = new XIAMultiRouteData();
        _rts[*dest] = list;
        _rts[*dest].push_back(xrd1);

        int port = _rtdata.port;
        if(strstr(_local_addr.unparse().c_str(), dest->unparse().c_str())) {
           port = DESTINED_FOR_LOCALHOST;
        }

        xrd1->port = port;
        xrd1->nexthop = newroute;
    }
    delete dest;
    */
    return -1;
}

int
XIAXIDMultiRouteTable::rescope(Packet *p, XID &xid)
{
    // add a XID to the next for last visited node
    // currently just unparse it into string, add xid and parse back
    // TODO: correctly add a node into a xiapath (should work for multiple edges)
    // TODO: do it fast without expensive calls (modify p directly?)
    click_chatter("XIAXIDMultiRouteTable:rescope %s", xid.unparse().c_str());

    const struct click_xia* hdr = p->xia_header();
    XIAHeader xiah(p->xia_header());

    click_chatter("rescope: orginal header: plen:%d, hlim:%d, nxt:%d", xiah.plen(), xiah.hlim(), xiah.nxt());

    XIAPath dst_path = xiah.dst_path(); //TODO: get rid of these expensive call
    XIAHeaderEncap encap(xiah);

    String dst_string = dst_path.unparse_re();
    click_chatter("rescope: dst string %s", dst_string.c_str());

    int last = hdr->last; // current visited node
    XID visited; //last visited node
    String visited_string = "";
    int index = 0;

    if (last >= 0){
        click_chatter("rescope: last: %d", last);
        visited = XID((*(hdr->node + last)).xid);
        visited_string = visited.unparse();
        index = dst_string.find_left(visited_string);
    } else{
        // source node
    }


    click_chatter("rescope: index %d find str %s", index, visited_string.c_str());

    index += visited_string.length();
    click_chatter("rescope: cut at index %d", index);

    String prefix = dst_string.substring(0, index);
    String postfix = dst_string.substring(index);

    String new_name = xid.unparse() + " ";

    String new_path_string = prefix + " " + new_name + postfix;
    click_chatter("rescope: pre %s, post %s, new name %s", prefix.c_str(), postfix.c_str(), new_name.c_str());

    click_chatter("rescope: new dst_path: %s", new_path_string.c_str());

    XIAPath new_path;
    new_path.parse_re(new_path_string);
    encap.set_dst_path(new_path);
    encap.encap_replace(p);
    click_chatter("%s rescope:done", _local_hid.unparse().c_str());

    return 0;
}

int
XIAXIDMultiRouteTable::lookup_route(int in_ether_port, Packet *p)
{
    //click_chatter("XIAXIDMultiRouteTable:lookup_route");

    //TODO: bind AD?
    const struct click_xia* hdr = p->xia_header();
    int last = hdr->last;
    bool rescoped = false; // if the dst path is already binded a AD?
    String rescoped_name = "";
    if (last < 0){
        last += hdr->dnode;
    }
    else{
        rescoped = true; // This is a shortcut for inter-domain SID. Should have better mechanism. Will not work for AD HID SID bind. TODO: actually check the value of the node
        rescoped_name = XID(hdr->node[last].xid).unparse(); // get the last node's name
    }
    const struct click_xia_xid_edge* edge = hdr->node[last].edge;
    const struct click_xia_xid_edge& current_edge = edge[XIA_NEXT_PATH_ANNO(p)];
    const int& idx = current_edge.idx;
    if (idx == CLICK_XIA_XID_EDGE_UNUSED)
    {
        // unused edge -- use default route
        return _rtdata.port;
    }

    const struct click_xia_xid_node& node = hdr->node[idx];

    XIAHeader xiah(p->xia_header());

    if (_bcast_xid == node.xid) {
        // Broadcast packet

        XIAPath source_path = xiah.src_path();
        source_path.remove_node(source_path.destination_node());
        XID source_hid = source_path.xid(source_path.destination_node());

        if(_local_hid == source_hid) {
                // Case 1. Outgoing broadcast packet: send it to port 7 (which will duplicate the packet and send each to every interface)
                p->set_nexthop_neighbor_xid_anno(_bcast_xid);
                return DESTINED_FOR_BROADCAST;
        } else {
            // Case 2. Incoming broadcast packet: send it to port 4 (which eventually send the packet to upper layer)
            // Also, mark the incoming (ethernet) interface number that connects to this neighbor
            // Disable the marking process as it may cause load balancing fall
            // TODO: fix this functionality
            /*
            HashTable<XID, XIARouteData*>::const_iterator it = _rts.find(source_hid);
            if (it != _rts.end())
              {
                if ((*it).second->port != in_ether_port) {
                  // update the entry
                  (*it).second->port = in_ether_port;
                }
              }
            else
              {
                // Make a new entry for this newly discovered neighbor
                XIARouteData *xrd1 = new XIARouteData();
                xrd1->port = in_ether_port;
                xrd1->nexthop = new XID(source_hid);
                _rts[source_hid] = xrd1;
              }*/
            return DESTINED_FOR_LOCALHOST;
        }
        // TODO: not sure what this should be??
        assert(0);
        return DESTINED_FOR_LOCALHOST;

    } else {
        // Unicast packet
        //click_chatter("XIAXIDMultiRouteTable:lookup_route: unicast");
        HashTable<XID, Vector<XIAMultiRouteData*> >::const_iterator it = _rts.find(node.xid);
        if (it != _rts.end() && it.value().size() != 0 )// TODO: if scoped by AD or HID these conditions does not guarantee to find a valid entry within that scope, need function to jump to default route.
        {
            //click_chatter("XIAXIDMultiRouteTable:lookup_route: 1 or more");
            XIAMultiRouteData *xrd = *(it.value().begin());
            if (it.value().size() > 1){// if actually has multiple routes
                //click_chatter("XIAXIDMultiRouteTable:lookup_route: 2 or more");
                #if CLICK_LINUXMODULE
                u32 random = random32()%100;
                #else
                int random = rand()%100;
                #endif
                click_chatter("XIAXIDMultiRouteTable:lookup_route: 2 or more: random number is %d", random);
                // TODO: faster/efficient way to do random selection
                // TODO: other selecting mechanism
                Vector<XIAMultiRouteData*>::const_iterator it_entry = it.value().begin();
                bool lookup_success = false; // whether the rescoped ine matches one of the candidates, it is to detect a illegal rescoped
                bool random_success = false; // already decision which one to pick?
                while (it_entry != it.value().end() ){
                    XIAMultiRouteData *xrd_candidate = *it_entry;
                    if (rescoped && hdr->node[last].xid.type == htonl(CLICK_XIA_XID_TYPE_HID))// if bind to a HID
                    {
                        if ( xrd_candidate->port == -2){ // local host entry NOTE: this is a hack. No idea if it's the right thing to do
                            xrd = xrd_candidate;
                            lookup_success = true;
                            break;
                        }
                    }
                    else if (rescoped){ // only search for the rescoped one (local AD entry)
                        if ( xrd_candidate->index == rescoped_name ){
                            xrd = xrd_candidate;
                            lookup_success = true;
                            break;
                        }

                    }
                    // actually randomly select the candidates, no matter the packet is rescoped or not. 
                    // always do it so that we can pretend (if any) the unsuccessful rescope is not there and we rescope the DAG again
                    if (!random_success){ // if not find one yet, do the round robin
                        if (random <= xrd_candidate->weight){
                            xrd = xrd_candidate;
                            if (!rescoped) // if not rescoped, job is done!
                                break;
                            else // if resccoped, keep on looking for if it is valid
                                random_success = true; // pick this one, no more round robin
                        }
                        else{
                            random -= xrd_candidate->weight;
                        }
                    }

                    ++it_entry;
                }
                if (rescoped && lookup_success){
                    click_chatter("%s XIAXIDMultiRouteTable:lookup_route: get rescoped SID goto %s", _local_hid.unparse().c_str(), xrd->index.c_str());
                }else if (rescoped && !lookup_success){
                    click_chatter("%s XIAXIDMultiRouteTable:lookup_route: get illegal rescoped SID goto %s, goto %s instead", _local_hid.unparse().c_str(), rescoped_name.c_str(), xrd->index.c_str());
                    XID new_scope(xrd->index);
                    rescope(p, new_scope);
                }
                else{
                    click_chatter("%s XIAXIDMultiRouteTable:lookup_route: 2 or more: %s is selected", _local_hid.unparse().c_str(), xrd->index.c_str());
                    XID new_scope(xrd->index); // for sid routing the index is the string of the selected AD
                    rescope(p, new_scope);
                }

            }
            //click_chatter("XIAXIDMultiRouteTable:lookup_route: process route");
            // check if outgoing packet
            if(xrd->port != DESTINED_FOR_LOCALHOST && xrd->port != FALLBACK && xrd->nexthop != NULL) {
                p->set_nexthop_neighbor_xid_anno(*(xrd->nexthop));
            }
            return xrd->port;
        }
        else
        {
            // no match -- use default route
            // check if outgoing packet
            if(_rtdata.port != DESTINED_FOR_LOCALHOST && _rtdata.port != FALLBACK && _rtdata.nexthop != NULL) {
                p->set_nexthop_neighbor_xid_anno(*(_rtdata.nexthop));
            }
            if ( rescoped && hdr->node[last].xid.type ==  htonl(CLICK_XIA_XID_TYPE_HID) ){
                // if already bind to a hid, do not use the default route to send it somewhere else
                // NOTE: this is a hack, works for preventing redirect flood for sid:XROUTE.
                // NOTE: is this the right way for interpreting DAG? Does the visited node scope the following nodes?
                // NOTE: What about AD1->AD2->HID1->AD3->HID2->HID3->SID1, which part is invalid?
                return UNREACHABLE;
            }
            else{
                return _rtdata.port;
            }
        }
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAXIDMultiRouteTable)
ELEMENT_MT_SAFE(XIAXIDMultiRouteTable)

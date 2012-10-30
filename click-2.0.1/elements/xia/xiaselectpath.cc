/*
 * xiaselectpath.{cc,hh} -- select a path to process
 */

#include <click/config.h>
#include "xiaselectpath.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
#include <clicknet/xia.h>
CLICK_DECLS

XIASelectPath::XIASelectPath()
{
}

XIASelectPath::~XIASelectPath()
{
}

int
XIASelectPath::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String mode;

    int ret = cp_va_kparse(conf, this, errh,
                            "MODE", cpkP+cpkM, cpString, &mode,
                            cpEnd);
    if (!ret)
        return ret;

    if (mode == "first")
        _first = true;
    else if (mode == "next")
        _first = false;
    else
        return errh->error("unrecognized mode: %s", mode.c_str());

    return 0;
}

void
XIASelectPath::push(int, Packet *p)
{
    if (_first) {
        SET_XIA_NEXT_PATH_ANNO(p, 0);
        output(0).push(p);
    }
    else {
        int next = XIA_NEXT_PATH_ANNO(p) + 1;
        SET_XIA_NEXT_PATH_ANNO(p, next);

		// had any path to consider?
        if (next < CLICK_XIA_XID_EDGE_NUM)
            output(0).push(p);
        else
            output(1).push(p);
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIASelectPath)
ELEMENT_MT_SAFE(XIASelectPath)

/*
 * xiaxidinfo.{cc,hh} -- element that registers XID as names
 */

#include <click/config.h>
#include <click/standard/xiaxidinfo.hh>
#include <click/nameinfo.hh>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/xiaheader.hh>
CLICK_DECLS

XIAXIDInfo::XIAXIDInfo()
{
}

XIAXIDInfo::~XIAXIDInfo()
{
}

int
XIAXIDInfo::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int before = errh->nerrors();

    for (int i = 0; i < conf.size(); i++) {
	Vector<String> parts;
	cp_spacevec(conf[i], parts);
	if (parts.size() == 0)
	    // allow empty arguments
	    continue;
	if (parts.size() != 2)
	    errh->error("expected %<NAME XID%>");

        union {
            struct {
                struct click_xia_xid a;
            } xia;
            char c[24];
        } x;
        if (cp_xid(parts[1], &x.xia.a))
        {
            struct click_xia_xid temp;
            if (query_xid(parts[0], &temp, this))
                return errh->error("\"%s\" duplicate name", parts[0].c_str());
            if (!NameInfo::define(NameInfo::T_XIA_XID, this, parts[0], &x.xia.a, sizeof(x.xia.a)))
                errh->error("\"%s\" failed to register", parts[0].c_str());
            //printf("%s : %s\n", parts[0].c_str(), parts[1].c_str());
        }
        else
            errh->error("\"%s\" %<%s%> is not a recognizable address", parts[0].c_str(), parts[1].c_str());
    }

    return (errh->nerrors() == before ? 0 : -1);
}

bool
XIAXIDInfo::query_xid(const String& s, struct click_xia_xid* store, const Element *e)
{
    return NameInfo::query(NameInfo::T_XIA_XID, e, s, store, sizeof(struct click_xia_xid));
}

String
XIAXIDInfo::revquery_xid(const struct click_xia_xid* store, const Element *e)
{
    return NameInfo::revquery(NameInfo::T_XIA_XID, e, store, sizeof(struct click_xia_xid));
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAXIDInfo)
ELEMENT_HEADER(<click/standard/xiaxidinfo.hh>)

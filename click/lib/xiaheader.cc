// -*- related-file-name: "../include/click/xiaheader.hh" -*-
/*
 */

#include <click/config.h>
#include <click/string.hh>
#include <click/xiaheader.hh>
#if CLICK_USERLEVEL
# include <unistd.h>
#endif
CLICK_DECLS

/** @file xiaheader.hh
 * @brief The Packet class models packets in Click.
 */

XIAHeader::~XIAHeader()
{
    std::vector<XIDGraphNode*>::iterator it;
    for (it= nodelist.begin();it!=nodelist.end();++it) {
        delete *it;
    }
}

/** @brief returns a string representation of the packet header */
String XIAHeader::toString()
{
    /* TODO  fill this part */
    String str=""; 
    return str;
}
CLICK_ENDDECLS

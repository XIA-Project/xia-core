#ifndef XIARENDEZVOUS_H
#define XIARENDEZVOUS_H

#include <click/xiasecurity.hh>
#include <click/xiapath.hh>
#include <clicknet/xia.h>

// Migrate messages definitions
#define MAX_RV_TIMESTAMP_STR_SIZE 64

// Public functions
bool build_rv_control_message(XIASecurityBuffer &rv_control_msg,
		String &hid, String &dag, String &timestamp);

/*
bool valid_rv_control_message(XIASecurityBuffer &rv_control_msg,
        XIAPath their_addr, XIAPath our_addr,
        XIAPath &accepted_addr, String &rv_control_ts);

bool build_rv_controlack_message(XIASecurityBuffer &rv_controlack_msg,
        XIAPath our_addr, XIAPath their_addr, String timestamp);

bool valid_rv_controlack_message(XIASecurityBuffer &rv_controlack_msg,
        XIAPath their_addr, XIAPath our_addr, String timestamp);

*/
#endif // XIARENDEZVOUS_H

#include <stdio.h>
#include <iostream>
#include "controller.h"
#include "../click-2.0.1/include/clicknet/xia.h"

int main(void)
{
	XcacheController ctrl;
	XcacheMeta meta;
	char sid_string[strlen("SID:") + XIA_SHA_DIGEST_STR_LEN];

	if(XmakeNewSID(sid_string, sizeof(sid_string))) {
		std::cout << "Could not allocate SID for xcache\n";
		return -1;
	}

	// FIXME: Hardcoded name
	remove("/tmp/xcache.socket");

	ctrl.run();
	return 0;
}

bool operator<(const struct click_xia_xid& x, const struct click_xia_xid& y)
{
	if(x.type < y.type) {
		return true;
	} else if(x.type > y.type) {
		return false;
	}

	for(int i = 0; i < CLICK_XIA_XID_ID_LEN; i++) {
		if(x.id[i] < y.id[i])
			return true;
		else if(x.id[i] > y.id[i])
			return false;
	}

	return false;
}


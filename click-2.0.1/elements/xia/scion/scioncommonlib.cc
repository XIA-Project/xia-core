#include <click/config.h>
#include <time.h>
#include "scioncommonlib.hh"
#include <stdio.h>
#include <string.h>

CLICK_DECLS

//GCD computation
int SCIONCommonLib::GCD(int x, int y) {

	if(y==0)
		return x;
	else
		return GCD(y, x%y);
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(SCIONCommonLib)

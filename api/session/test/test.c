#include <stdio.h>
#include <sys/socket.h>
#include "session.h"

int main()
{
	int ctx = SnewContext();
	Sinit(ctx, "one, two, three", "return1,return2,return3", "one");
	return 0;
}

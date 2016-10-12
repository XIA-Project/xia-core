#include "RouteModule.hh"
#include <syslog.h>

pthread_t RouteModule::start()
{
	int rc;

	init();
	_enabled = true;

	// connect to the click route engine
	_xr.setRouter(_hostname);
	if ((rc = _xr.connect()) != 0) {
		syslog(LOG_ALERT, "unable to connect to click (%d)", rc);
		exit(-1);
	}

	rc = pthread_create(&_t, NULL, run, (void*)this);
	if (rc == 0) {
		return _t;
	} else {
		return 0;
	}
}

void *RouteModule::run(void *inst)
{
	RouteModule *re = (RouteModule*)inst;
	while (re->_enabled) {
		re->handler();
	}
	return NULL;
}

int RouteModule::wait()
{
	return pthread_join(_t, NULL);
}

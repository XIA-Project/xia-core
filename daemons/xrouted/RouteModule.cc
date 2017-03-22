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

int RouteModule::sendMessage(sockaddr_x *dest, const Xroute::XrouteMsg &msg)
{
	int rc;
	string message;
	msg.SerializeToString(&message);

	rc = Xsendto(_source_sock, message.c_str(), message.length(), 0, (sockaddr*)dest, sizeof(sockaddr_x));
	if (rc < 0) {
		syslog(LOG_WARNING, "unable to send %s msg: %s", Xroute::msg_type_Name(msg.type()).c_str(), strerror(errno));
	}
	return rc;
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

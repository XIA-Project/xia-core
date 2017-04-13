#include "RouteModule.hh"
#include <syslog.h>


RouteModule::RouteModule(const char *name)
{
	_hostname       = name;
	_broadcast_sock = 0;
	_local_sock     = 0;
	_source_sock    = 0;
}
pthread_t RouteModule::start()
{
	int rc;

	// connect to the click route engine
	_xr.setRouter(_hostname);
	if ((rc = _xr.connect()) != 0) {
		syslog(LOG_ALERT, "unable to connect to click (%d)", rc);
		exit(-1);
	}

	makeLocalSocket();

	init();
	_enabled = true;

	rc = pthread_create(&_t, NULL, run, (void*)this);
	if (rc == 0) {
		return _t;
	} else {
		return 0;
	}
}


int RouteModule::makeLocalSocket()
{
	struct sockaddr_in sin;

	inet_pton(AF_INET, "127.0.0.1", &sin.sin_addr);
	sin.sin_port = htons(LOCAL_PORT);
	sin.sin_family = AF_INET;

	_local_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (_local_sock < 0) {
		syslog(LOG_ALERT, "Unable to create the local control socket");
		return -1;
	}
	if (bind(_local_sock, (struct sockaddr*)&sin, sizeof(struct sockaddr)) < 0) {
		syslog(LOG_ALERT, "unable to bind to localhost:%u", LOCAL_PORT);
		return -1;
	}

	return 0;
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

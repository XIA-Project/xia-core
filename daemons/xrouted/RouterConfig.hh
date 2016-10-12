#ifndef _RouterConfig_hh
#define _RouterConfig_hh

#define APPNAME "xrouted"
#define EXPIRE_TIME 60

// types of router protocols we may handle
const unsigned SDN_ROUTER   = 0x01;
const unsigned LOCAL_ROUTER = 0x02;
const unsigned SID_ROUTER   = 0x04;
const unsigned CID_ROUTER   = 0x08;
const unsigned OLD_ROUTER   = 0x10;

class RouterConfig {
public:
	RouterConfig();
	~RouterConfig();
	int parseCmdLine(int argc, char**argv);
	void help();

	const char *hostname()	{ return _hostname; }
	const char *appname()	{ return _appname; }
	const char *ident()		{ return _ident; }
	bool verbose()			{ return _verbose; }
	unsigned logLevel()		{ return _loglevel; }
	unsigned routers()		{ return _routers; }

private:
	char _hostname[24];
	char _appname[24];
	char _ident[48];

	bool _verbose;
	unsigned _loglevel;
	unsigned _routers;
};

#endif

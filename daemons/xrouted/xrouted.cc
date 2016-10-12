#include <syslog.h>
#include <signal.h>
#include "RouterConfig.hh"
#include "RouteModule.hh"
#include "IntraDomainRouter.hh"
#include "Controller.hh"

// FIXME: we need to stop LSA and Route Table broadcasts from leaving our AD

RouterConfig config;
ModuleList modules;

void initLogging()
{
	unsigned flags = LOG_CONS|LOG_NDELAY|LOG_LOCAL4;

	if (config.verbose()) {
		flags |= LOG_PERROR;
	}
	openlog(config.ident(), flags, LOG_LOCAL4);
	setlogmask(LOG_UPTO(config.logLevel()));
	syslog(LOG_NOTICE, "%s started on %s", config.appname(), config.hostname());
}

void term(int /*signum*/)
{
	ModuleList::iterator it;
	for (it = modules.begin(); it != modules.end(); it++) {
		(*it)->stop();
	}
}

int main(int argc, char *argv[])
{
	if (config.parseCmdLine(argc, argv) < 0) {
		exit(-1);
	}

	initLogging();

	// FIXME: unfortunately with the below code, if click crashes,
	// the threads get stuck in a read calls and don't exit
	//	look at using ppoll instead of poll in the API call
	// graceful exit of threads
	// struct sigaction action;
	// memset(&action, 0, sizeof(struct sigaction));
	// action.sa_handler = term;
	// sigaction(SIGTERM, &action, NULL);

	if (config.routers() & LOCAL_ROUTER) {
		IntraDomainRouter *r = new IntraDomainRouter(config.hostname());
		r->start();
		printf("router\n");
		modules.push_back(r);
	}
	if (config.routers() & SDN_ROUTER) {
		Controller *c = new Controller(config.hostname());
		c->start();
		printf("controller\n");
		modules.push_back(c);
	}

	// wait for everyone to quit
	ModuleList::iterator it;
	for (it = modules.begin(); it != modules.end(); it++) {
		(*it)->wait();
	}
	return 0;
}

/*
** Copyright 2017 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
#include <syslog.h>
#include <signal.h>
#include "RouterConfig.hh"
#include "RouteModule.hh"
#include "Host.hh"

RouterConfig config;
Host *h = NULL;

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
	h->stop();
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
	// sigemptyset(&action.sa_mask);
	// sigaction(SIGTERM, &action, NULL);

	// // Block SIGTERM, we'll only see it when in
	// sigset_t sigset, oldset;
	// sigemptyset(&sigset);
	// sigaddset(&sigset, SIGTERM);
	// sigprocmask(SIG_BLOCK, &sigset, &oldset);


	h = new Host(config.hostname());
	return h->run();
}

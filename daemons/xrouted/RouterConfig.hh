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
#ifndef _RouterConfig_hh
#define _RouterConfig_hh

#define APPNAME "xrouted"

// how long to wait until we purge missing peer routes
#define NEIGHBOR_EXPIRE_TIME 10
#define ROUTE_EXPIRE_TIME    60

// how often to send keep alive messages
#define KEEPALIVE_SECONDS 1
#define KEEPALIVE_MICROSECONDS 0

// how often to send LSAs
// If a route table change happens, a new LSA will be sent immediatelyi
//  regardless of how much time is remaining in the timer
#define LSA_SECONDS 5
#define LSA_MICROSECONDS 0

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

private:
	char _hostname[24];
	char _appname[24];
	char _ident[48];

	bool _verbose;
	unsigned _loglevel;
	unsigned _routers;
};

#endif

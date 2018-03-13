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
#define EXPIRE_TIME 60

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

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
#include "Settings.hh"
#include "Xsocket.h"
#include "minIni.h"

Settings::Settings(const char *h)
{
	char full_path[BUF_SIZE];
	char root[BUF_SIZE];

    _hostname = h;

	snprintf(full_path, BUF_SIZE, "%s/%s", XrootDir(root, BUF_SIZE), SETTINGS_FILE);
    _settings_file = full_path;

    _expire_time            = EXPIRE_TIME;
    _hello_interval         = HELLO_INTERVAL;
    _lsa_interval           = LSA_INTERVAL;
    _sid_discovery_interval = SID_DISCOVERY_INTERVAL;
    _sid_decision_interval  = SID_DECISION_INTERVAL;
    _ad_lsa_interval        = AD_LSA_INTERVAL;
    _calc_dijkstra_interval = CALC_DIJKSTRA_INTERVAL;
    _update_latency         = UPDATE_LATENCY;
    _update_config          = UPDATE_CONFIG;
    _max_hop_count          = MAX_HOP_COUNT;
    _enable_sid_ctl         = ENABLE_SID_CTL;
}

void Settings::reload()
{
	char section_name[BUF_SIZE];
	bool mysection = false; // read default section or my section
	int section_index = 0;

	while (ini_getsection(section_index, section_name, BUF_SIZE, _settings_file.c_str())) {
		if (strcmp(section_name, _hostname.c_str()) == 0) {
            // section for me
			mysection = true;
			break;
		}
		section_index++;
	}

	if (mysection) {
		strcpy(section_name, _hostname.c_str());
	} else {
		strcpy(section_name, "default");
	}

	_expire_time             = ini_getl(section_name, "expire_time", EXPIRE_TIME, _settings_file.c_str());
	_hello_interval          = ini_getf(section_name, "hello_interval", HELLO_INTERVAL, _settings_file.c_str());
	_lsa_interval            = ini_getf(section_name, "LSA_interval", LSA_INTERVAL, _settings_file.c_str());
	_sid_discovery_interval  = ini_getf(section_name, "SID_discovery_interval", SID_DISCOVERY_INTERVAL, _settings_file.c_str());
	_sid_decision_interval   = ini_getf(section_name, "SID_decision_interval", SID_DECISION_INTERVAL, _settings_file.c_str());
	_ad_lsa_interval         = ini_getf(section_name, "AD_LSA_interval", AD_LSA_INTERVAL, _settings_file.c_str());
	_calc_dijkstra_interval  = ini_getf(section_name, "calc_Dijkstra_interval", CALC_DIJKSTRA_INTERVAL, _settings_file.c_str());
	_max_hop_count           = ini_getl(section_name, "max_hop_count", MAX_HOP_COUNT, _settings_file.c_str());
	_update_config           = ini_getl(section_name, "update_config", UPDATE_CONFIG, _settings_file.c_str());
	_update_latency          = ini_getl(section_name, "update_latency", UPDATE_LATENCY, _settings_file.c_str());
	_enable_sid_ctl          = ini_getl(section_name, "enable_SID_ctl", ENABLE_SID_CTL, _settings_file.c_str());
}

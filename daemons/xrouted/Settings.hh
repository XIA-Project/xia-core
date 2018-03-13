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
#ifndef _settings_hh
#define _settings_hh

#include <string>

#define EXPIRE_TIME            60
#define HELLO_INTERVAL         0.1
#define LSA_INTERVAL           0.3
#define SID_DISCOVERY_INTERVAL 3.0
#define SID_DECISION_INTERVAL  5.0
#define AD_LSA_INTERVAL        1
#define CALC_DIJKSTRA_INTERVAL 4
#define MAX_HOP_COUNT          50
#define UPDATE_LATENCY         60
#define UPDATE_CONFIG          5
#define ENABLE_SID_CTL         0
#define SETTINGS_FILE          "etc/controllers.ini"


class Settings {
public:
    Settings(const char *h);
    void reload();

    int    expire_time()            { return _expire_time; };
    double hello_interval()         { return _hello_interval; };
    double lsa_interval()           { return _lsa_interval; };
    double sid_discovery_interval() { return _sid_discovery_interval; };
    double sid_decision_interval()  { return _sid_decision_interval; };
    double ad_lsa_interval()        { return _ad_lsa_interval; };
    double calc_dijkstra_interval() { return _calc_dijkstra_interval; };
    int    update_latency()         { return _update_latency; };
    int    update_config()          { return _update_config; };
    int    max_hop_count()          { return _max_hop_count; };
    int    enable_sid_ctl()         { return _enable_sid_ctl; };

private:
    std::string _settings_file;
    std::string _hostname;

    int    _expire_time;
    double _hello_interval;
    double _lsa_interval;
    double _sid_discovery_interval;
    double _sid_decision_interval;
    double _ad_lsa_interval;
    double _calc_dijkstra_interval;
    int    _update_latency;
    int    _update_config;
    int    _max_hop_count;
    int    _enable_sid_ctl;
};

#endif


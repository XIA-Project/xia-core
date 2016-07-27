#ifndef _CONSTANTS_H
#define _CONSTANTS_H

// constants definition related to workload generator
const char* CONFIG_DIR = "conf";
const char* WORKLOAD_DIR = "workload";
const char* CID_MAP_DIR = "tmp";
const char* STATS_DIR = "stats";

const char* SERVER_WORKLOAD_FILE_TEMPLATE = "sworkload_s";
const char* CLIENT_WORKLOAD_FILE_TEMPLATE = "cworkload_c";
const char* CID_MAP_FILE_TEMPLATE = "id2cid_s";
const char* CLIENT_STATS_FILE_TEMPLATE = "stats_c";


const char* CONFIG_FILE_NAME = "conf/workload.conf";
const char* SERVER_WORKLOAD_TEMPLATE_FILE_NAME = "workload/sworkload_s";
const char* CLIENT_WORKLOAD_TEMPLATE_FILE_NAME = "workload/cworkload_c";

// definition related to server
const int MAX_XID_SIZE = 100;
const char* XIA_CID_ROUTING_SERVER_NAME = "www.server.xia";

#endif
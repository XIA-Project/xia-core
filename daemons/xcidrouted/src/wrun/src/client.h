#ifndef _CLIENT_H
#define _CLIENT_H

#include <sstream>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>
#include <iostream>

#include "xcache.h"
#include "Xsocket.h"
#include "dagaddr.hpp"
#include "dagaddr.h"
#include "Xkeys.h"

#define MB(__mb) (KB(__mb) * 1024)
#define KB(__kb) ((__kb) * 1024)

void parseChunkIdToCIDMap(string cidMapDir);

void fetchWorkload(ClientWorkloadParser* parser);

void saveStats(int workloadId);

#endif
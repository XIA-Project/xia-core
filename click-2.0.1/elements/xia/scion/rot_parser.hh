/*
 * Copyright (C) 2012-2016 Carnegie Mellon CyLab
 * 
 * name: ROTParser.hh
 * description: 
 * version: 0.9
 *
 */

#ifndef ROT_PARSER_HH_
#define ROT_PARSER_HH_
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <map>
#include "tinyxml2.hh"

using namespace std;
using namespace tinyxml2;


enum ROTlogyError{
	ROTParseNoError=0,	// no errors
	ROTParseFail,		// parsing error
	ROTVerifyFail,		// verification error
	ROTNotExist			// rot is missing
};

// coreAD structure
struct CoreAD{
	uint64_t aid;
	int certLen;
	char* certificate;
};

// ROT structure
struct ROT{
	int version;
	struct tm expiredate;
	struct tm issuedate;
	int policyThreshold;
	int certThreshold;
	int numCoreAD;
	uint64_t TDID;
	map<uint64_t, CoreAD> coreADs;
};

class ROTParser{
    
public :     
	ROTParser():m_bIsInit(false){}
	// load methods, from file or char* buffer
	int loadROTFile(const char* path);
	int loadROT(const char* rotFile);
	// Verify ROT file itself based on attached signatures and public keys
	int verifyROT(const ROT &rot);
	// parse ROT file or data buffer chunk 
	// parsed vaiables are stored in rot element
	int parse(ROT &rot); 
private:
	bool m_bIsInit;
	string m_sfilePath; 
	XMLDocument doc;
	// stuctures storing signatures and corresponding length
	map<uint64_t, int> sigLengths;
	map<uint64_t, uint8_t*> signatures;
};
#endif

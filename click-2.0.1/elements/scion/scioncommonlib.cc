/*
** Copyright 2013 Carnegie Mellon University / ETH Zurich
** 
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
** 
** http://www.apache.org/licenses/LICENSE-2.0 
** 
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

#include <click/config.h>
#include <time.h>
#include "scioncommonlib.hh"
#include "packetheader.hh"
#include <stdio.h>
#include <string.h>

CLICK_DECLS

//GCD computation
int SCIONCommonLib::GCD(int x, int y) {

	if(y==0)
		return x;
	else
		return GCD(y, x%y);
}

/*
	Save a packet's payload to a file
*/
int
SCIONCommonLib::saveToFile(SPacket *packet, uint16_t offset, uint16_t packetLength, String &fn){
	uint8_t srcLen = SPH::getSrcLen(packet);
	uint8_t dstLen = SPH::getDstLen(packet);
	// open a file with 0 size
	int rotLen = packetLength-SPH::getHdrLen(packet) -offset;
			
	// Write to file defined in Config
	FILE* fp = fopen(fn.c_str(), "w+");
	if(!fp)
		return SCION_FAILURE;

	fwrite(SPH::getData(packet)+offset, 1, rotLen, fp);
	fclose(fp);
	return SCION_SUCCESS;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(SCIONCommonLib)

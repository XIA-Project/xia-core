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

#ifndef SCIONCOMMONLIB_HH_INCLUDED
#define SCIONCOMMONLIB_HH_INCLUDED

#define SCL SCIONCommonLib

#include "define.hh"
#include "packetheader.hh"
#include <string.h>
#include <click/string.hh>

class SCIONCommonLib{

    public :
        static int GCD(int x, int y); 
		static int saveToFile(SPacket * pkt, uint16_t offset, uint16_t packetLength, String &fn);

};
#endif

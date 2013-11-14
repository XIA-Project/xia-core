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

#include <clicknet/ip.h>
#include <netinet/ip.h>
#include "define.hh"

class SCIONIPDecap {
	public:
		SCIONIPDecap();
		~SCIONIPDecap();

		void initialize();
		
		int decap(uint8_t * ipp, uint16_t &len); 
		//Currently, we assume only 20B header is added to the SCION packet for IPDecap
		//i.e., no option header is present
		int getHeaderLen() {return IPHDR_LEN;}

	private:
		inline int verify_checksum(uint8_t *ip, int off) const;
		uint16_t cksum(uint8_t *ip, uint16_t len);
		struct ip m_iph;
		uint16_t m_id;
};



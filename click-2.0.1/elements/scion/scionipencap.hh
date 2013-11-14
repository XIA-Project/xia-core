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

#ifndef SCION_IPENCAP_HH
#define SCION_IPENCAP_HH

#include <clicknet/ip.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include "define.hh"


class SCIONIPEncap {
	public:
		SCIONIPEncap(){}
		~SCIONIPEncap(){}

		void initialize(uint32_t ip_src, uint32_t ip_dst = 0); //prepare IP header to expedite IP encap
		
		int encap(uint8_t * ipp, uint8_t * packet, uint16_t &len, uint32_t dst=0); 
		//Currently, we assume only 20B header is added to the SCION packet for IPEncap
		//i.e., no option header is present
		int getHeaderLen() {return IPHDR_LEN;}

	private:
		inline void update_checksum(struct ip *ip, int off) const;
		uint16_t cksum(uint8_t *ip, uint16_t len);
		struct ip m_iph;
		struct udphdr m_udph;
		uint16_t m_id;
};

#endif

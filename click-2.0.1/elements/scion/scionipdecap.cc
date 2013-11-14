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

#include <scionipdecap.hh>
#include <arpa/inet.h>
#include <clicknet/ip.h>

SCIONIPDecap::SCIONIPDecap () {

}

SCIONIPDecap::~SCIONIPDecap() {

}

void SCIONIPDecap::initialize() {
	m_id = 0;
	uint8_t ip_version = 4;
	uint8_t ip_header_len = 20;

	memset(&m_iph, 0, sizeof(ip));
	m_iph.ip_src = ; //read from configuration
	m_iph.ip_dst = ; //read from configuration
	m_iph.ip_tos = 0;
	m_iph.ip_len = htons(0); //set packet length
	m_iph.ip_id = htons(m_id);
	m_iph.ip_off = htons(0);
	m_iph.ip_ttl = 253; //SL: just random selection to make difference from others
	//m_iph.ip_p = 253; //protocol: currently set to IPv4 encap (need to be changed later)
	m_iph.ip_p = SCION_PROTO_NUM; //experimental
	m_iph.ip_sum; //ip checksum

	#ifdef _IP_VHL
	m_ihp.ip_vhl = ip_version << 4 | m_iph.hl >> 2; /* version << 4 | header length >> 2 */
	#else
	/*little endian: ip_hl:4, ip_v:4
	     big endian: ip_v:4, ip_hl:4 */
	m_iph.ip_v = 4; //version
	m_iph.ip_hl = sizeof(ip) >> 2; //header length
	#endif  
	
	//The following code makes use of Click implementation
	// set the checksum field so we can calculate the checksum incrementally
#if HAVE_FAST_CHECKSUM
  	m_iph.ip_sum = ip_fast_csum((unsigned char *) &_iph, sizeof(click_ip) >> 2);
#else
 	 m_iph.ip_sum = click_in_cksum((unsigned char *) &_iph, sizeof(click_ip));
#endif


}

/*
	prepend IP header
*/
int SCIONIPDecap::decap(uint8_t * ipp, uint16_t &len) {
	//memory needs to be released by the caller
	if(verify_checksum(ipp, 0) == SCION_FAILURE) {
	//need to check if this can be done incrementally using the offset
		return SCION_FAILURE;
	}
	memmove(ipp, ipp+IPHDR_LEN,IPHDR_LEN);
	len -= IPHDR_LEN;
	return SCION_SUCCESS;
}

inline int 
SCIONIPDecap::verify_checksum(struct ip * ip, int off) const
{
	//should perform checksum verification here
	if(ip->ip_chk != cksum(ip,IPHDR_LEN))
		return SCION_FAILURE;
	
	return SCION_SUCESS;
}

uint16_t SCIONIPDecap::cksum(uint8_t *ip, uint16_t len){
	uint16_t cksum;
	uint32_t sum = 0; 

	while(len > 1){
		sum += *((uint16_t*) ip)++;
		if(sum & 0x80000000)   /* if high order bit set, fold */
			sum = (sum & 0xFFFF) + (sum >> 16);
		len -= 2;
	}

	if(len)       /* take care of left over byte */
		sum += (uint16_t) *(unsigned char *)ip;
	
	while(sum>>16)
		sum = (sum & 0xFFFF) + (sum >> 16);
	
	cksum = (uint16_t) ~sum;
	return chsum;
}

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

#include "scionipencap.hh"
#include <arpa/inet.h>
#include <clicknet/ip.h>
#include <click/config.h>
#include "packetheader.hh"

CLICK_DECLS

/*
SCIONIPEncap::SCIONIPEncap () {

}

SCIONIPEncap::~SCIONIPEncap() {

}
*/

void SCIONIPEncap::initialize(uint32_t ip_src, uint32_t ip_dst) {
	m_id = 0;
	uint8_t ip_version = 4;
	uint8_t ip_header_len = 20;

	memset(&m_iph, 0, sizeof(ip));
	//SL: address needs to be set here
	//src address should be initialized by an element (who owns this address)
	//dst address can either be set (e.g., tunneling) or be dynamically changed according to
	//the configuration
	m_iph.ip_src.s_addr = ip_src; //read from configuration
	m_iph.ip_dst.s_addr = ip_dst; //read from configuration

	m_iph.ip_tos = 0;
	m_iph.ip_len = htons(0); //set packet length
	m_iph.ip_id = htons(m_id);
	m_iph.ip_off = htons(0);
	m_iph.ip_ttl = 253; //SL: just random selection to make difference from others
	//m_iph.ip_p = 253; //protocol: currently set to IPv4 encap (need to be changed later)
	m_iph.ip_p = SCION_PROTO_NUM; //set to experimental
	//m_iph.ip_sum = 0; //ip checksum

	#ifdef _IP_VHL
	m_ihp.ip_vhl = ip_version << 4 | m_iph.hl >> 2; /* version << 4 | header length >> 2 */
	#ifdef _SL_DEBUG_ENCAP
	printf("IPVHL is set\n");
	#endif
	#else
	/*little endian: ip_hl:4, ip_v:4
	     big endian: ip_v:4, ip_hl:4 */
	m_iph.ip_v = 4; //version
	m_iph.ip_hl = sizeof(ip) >> 2; //header length
	#ifdef _SL_DEBUG_ENCAP
	printf("IPVHL is not set: ip_v=%d, ip_hl=%d\n", m_iph.ip_v, m_iph.ip_hl);
	#endif
	#endif  
	
	//The following code makes use of Click implementation
	// set the checksum field so we can calculate the checksum incrementally
#if HAVE_FAST_CHECKSUM
  	m_iph.ip_sum = ip_fast_csum((unsigned char *) &m_iph, sizeof(click_ip) >> 2);
#else
 	 m_iph.ip_sum = click_in_cksum((unsigned char *) &m_iph, sizeof(click_ip));
#endif

	//SL:
	//UDP header initialize...
	m_udph.source = htons(SCION_PORT_NUM);
	m_udph.dest = htons(SCION_PORT_NUM);
	m_udph.len = htons(0);
	m_udph.check = 0;
}

/*
	prepend IP header
*/
int SCIONIPEncap::encap(uint8_t * ipp, uint8_t * packet, uint16_t &len, uint32_t dst) {
	//memory needs to be released by the caller

	if(packet == NULL)
		return SCION_FAILURE;

	memcpy(ipp+UDPIPHDR_LEN, packet, len); //entire scion packet becomes IP payload

	//Set IP header
	struct ip * iph = (struct ip *)ipp;
	memcpy(iph, &m_iph, IPHDR_LEN);
	if(dst)
		iph->ip_dst.s_addr = dst;
	iph->ip_id = htons(m_id++);
	len += UDPIPHDR_LEN;
	iph->ip_len = htons(len);

	//compute IP checksum
	update_checksum(iph, 2);
  	update_checksum(iph, 4);

	//SL: now add UDP header...
	//Note: leave checksum as zero -> see if it makes any trouble
	struct udphdr * udph = (struct udphdr *)(iph +1);
	memcpy(udph, &m_udph, UDPHDR_LEN);
	udph->len = htons(len - IPHDR_LEN);

	#ifdef _SL_DEBUG_ENCAP
	printf("IPENCAP: ip_hl=%d, ip_len=%d\n", iph->ip_hl,ntohs(iph->ip_len));
	#endif
	return SCION_SUCCESS;
}

inline void 
SCIONIPEncap::update_checksum(struct ip * ip, int off) const
{
#if HAVE_INDIFFERENT_ALIGNMENT
    click_update_in_cksum(&ip->ip_sum, 0, ((uint16_t *) ip)[off/2]);
#else
    const uint8_t *u = (const uint8_t *) ip;
# if CLICK_BYTE_ORDER == CLICK_BIG_ENDIAN
    click_update_in_cksum(&ip->ip_sum, 0, u[off]*256 + u[off+1]);
# else
    click_update_in_cksum(&ip->ip_sum, 0, u[off] + u[off+1]*256);
# endif
#endif
}

uint16_t SCIONIPEncap::cksum(uint8_t *ip, uint16_t len){
	uint16_t cksum;
	uint32_t sum = 0; 
	uint16_t * hdr = (uint16_t *)ip;

	while(len > 1){
		sum += *hdr;
		hdr++;
		if(sum & 0x80000000)   /* if high order bit set, fold */
			sum = (sum & 0xFFFF) + (sum >> 16);
		len -= 2;
	}

	if(len)       /* take care of left over byte */
		sum += (uint16_t) *(unsigned char *)ip;
	
	while(sum>>16)
		sum = (sum & 0xFFFF) + (sum >> 16);
	
	cksum = (uint16_t) ~sum;
	return cksum;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(SCIONIPEncap)


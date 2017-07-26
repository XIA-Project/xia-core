#ifndef _NCID_HEADER_H
#define _NCID_HEADER_H

#include "cid_header.h"
#include "publisher/publisher.h"

class NCIDHeader : public CIDHeader {
	public:
		// Call the CID Header constructor first
		NCIDHeader(const std::string &data, time_t ttl, std::string pub_name,
				std::string content_name);
	private:
		std::string _signature;
		std::string _uri;
};

/*!
 * Build an NCID Header on top of the CID Header
 */
NCIDHeader::NCIDHeader(const std::string &data, time_t ttl,
		std::string publisher_name, std::string content_name)
	: CIDHeader(data, ttl)
{
	Publisher publisher(publisher_name);
	_id = "NCID:" + publisher.ncid(content_name);
	_uri = publisher.content_URI(content_name);
	// TODO: 
	std::string signature;
	if(publisher.sign(_uri, data, signature)) {
		printf("ERROR: Unable to sign %s\n", _uri.c_str());
		// TODO throw an exception here
	}
	_signature = signature;
}
#endif // _NCID_HEADER_H

#ifndef _NCID_HEADER_H
#define _NCID_HEADER_H

#include "cid_header.h"
#include "publisher_list.h"
#include "publisher/publisher.h"

class NCIDHeader : public CIDHeader {
	public:
		// Call the CID Header constructor first
		NCIDHeader(const std::string &data, time_t ttl, std::string pub_name,
				std::string content_name);
		NCIDHeader(const std::string &buf);
		virtual ~NCIDHeader() {}
		virtual std::string serialize();
		virtual bool deserialize(const std::string &buf);
		virtual bool valid_data(const std::string &buf);

	private:
		std::string _signature;
		std::string _uri;
		std::string _publisher_name;
};

#endif // _NCID_HEADER_H

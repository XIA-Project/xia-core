#ifndef _CID_HEADER_H
#define _CID_HEADER_H

#include "Xsecurity.h"
#include "content_header.h"
#include "chdr.pb.h"

class CIDHeader : public ContentHeader {
	public:
		CIDHeader(const std::string &data, time_t ttl);

		// NCIDHeader() calls CIDHeader(), so ignore deserialize failure
		CIDHeader(const std::string &buf) { deserialize(buf); }

		virtual ~CIDHeader() {}

		virtual std::string id() { return _id; }
		virtual void set_id(std::string id) { _id = id; }

		virtual std::string store_id() { return _store_id; }
		virtual void set_store_id(std::string store_id) {
			_store_id=store_id;
		}

		virtual size_t content_len() { return _len; }
		virtual void set_content_len(size_t len) { _len = len;}

		virtual time_t ttl() { return _ttl; }
		virtual void set_ttl(time_t ttl) { _ttl = ttl; }
		virtual std::string serialize();
		virtual bool deserialize(const std::string &buf);

		virtual bool valid_data(const std::string &data);

	private:
		std::string _sha1hash(const std::string &data);
		std::string _calculate_id(const std::string &data);
};

#endif // _CID_HEADER_H

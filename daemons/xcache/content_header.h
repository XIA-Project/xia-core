#ifndef _CONTENT_HEADER_H
#define _CONTENT_HEADER_H

/*!
 * @brief Abstract class representing content headers
 *
 * Xcache controller should be using this abstraction to maintain content
 * headers. This allows us to modify the implementation of CID and NCID
 * headers appropriately.
 */

class ContentHeader {
	public:
		virtual ~ContentHeader() {}
		virtual std::string id() = 0;
		virtual void set_id(std::string id) = 0;

		virtual std::string store_id() = 0;
		virtual void set_store_id(std::string store_id) = 0;

		virtual size_t content_len() = 0;
		virtual void set_content_len(size_t len) = 0;

		virtual time_t ttl() = 0;
		virtual void set_ttl(time_t ttl) = 0;

		virtual std::string serialize() = 0;
		virtual bool deserialize(const std::string &buf) = 0;

		virtual bool valid_data(const std::string &data) = 0;

	protected:
		size_t _len;
		time_t _ttl;
		std::string _id;
		std::string _store_id;
};
#endif // _CONTENT_HEADER_H

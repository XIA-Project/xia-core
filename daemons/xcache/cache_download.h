#ifndef __CACHE_DOWNLOAD_H__
#define __CACHE_DOWNLOAD_H__

#include "meta.h"

#include <string>

class cache_download {
	public:
		void set_cid(std::string cid);
		std::string cid();
		void set_chdr_len(uint32_t chdr_len);
		uint32_t chdr_len();
		void set_chdr_data(char *chdr_data);
		char *chdr_data();
		void set_chdr(ContentHeader *chdr);
		ContentHeader *chdr();
		void set_data_buf(char *data);
		char *data_buf();
		void set_meta(xcache_meta *meta);
		xcache_meta *meta();
		cache_download();
		~cache_download();
		void destroy();
	private:
		std::string _cid;
		uint32_t _chdr_len;    // length of serialized content header
		char *_chdr_data;      // collect serialized content header
		ContentHeader *_chdr;  // deserialized content header
		char *_data;           // stream data content being cached
		xcache_meta *_meta;    // metadata for content being cached
};

#endif

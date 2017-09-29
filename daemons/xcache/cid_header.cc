#include "cid_header.h"

#include <iostream>

std::string CIDHeader::_sha1hash(const std::string &data)
{
	char hash[XIA_SHA_DIGEST_STR_LEN];
	xs_getSHA1HexDigest((const unsigned char *)data.c_str(), data.size(),
			hash, sizeof(hash));
	assert(strlen(hash) == XIA_SHA_DIGEST_STR_LEN - 1);
	std::string hashstr(hash);
	return hashstr;
}

std::string CIDHeader::_calculate_id(const std::string &data)
{
	return "CID:" + _sha1hash(data);
}

CIDHeader::CIDHeader(const std::string &data, time_t ttl)
{
	_len = data.size();
	_ttl = ttl;
	_id = _calculate_id(data);
	_store_id = _id;
}

std::string
CIDHeader::serialize()
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	ContentHeaderBuf chdr_buf;
	CIDHeaderBuf *cid_header = chdr_buf.mutable_cid_header();
	cid_header->set_len(_len);
	cid_header->set_ttl(_ttl);
	cid_header->set_id(_id);
	std::string serialized_header;
	chdr_buf.SerializeToString(&serialized_header);
	return serialized_header;
}

bool
CIDHeader::deserialize(const std::string &buf)
{
	ContentHeaderBuf chdr_buf;
	if(chdr_buf.ParseFromString(buf) == false) {
		std::cout << "CIDHeader unable to parse provided buffer" << std::endl;
		return false;
	}
	if(!chdr_buf.has_cid_header()) {
		return false;
	}
	const CIDHeaderBuf cid_hdr = chdr_buf.cid_header();
	_len = cid_hdr.len();
	_ttl = cid_hdr.ttl();
	_id = _store_id = cid_hdr.id();
	return true;
}

bool
CIDHeader::valid_data(const std::string &data)
{
	return (_calculate_id(data) == _id);
}

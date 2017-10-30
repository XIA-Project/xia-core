#include "ncid_header.h"

#include <iostream>

/*!
 * Build an NCID Header on top of the CID Header
 */
NCIDHeader::NCIDHeader(const std::string &data, time_t ttl,
		std::string publisher_name, std::string content_name)
	: CIDHeader(data, ttl)
{
	PublisherList *publishers = PublisherList::get_publishers();
	Publisher *publisher = publishers->get(publisher_name);

	_id = publisher->ncid(content_name);
	_uri = publisher->content_URI(content_name);
	_publisher_name = publisher_name;
	std::string signature;
	if(publisher->sign(_uri, data, signature)) {
		printf("ERROR: Unable to sign %s\n", _uri.c_str());
		// TODO throw an exception here
	}
	_signature = signature;
}

NCIDHeader::NCIDHeader(const std::string &buf)
	: CIDHeader(buf)
{
	deserialize(buf);
}

std::string
NCIDHeader::serialize()
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	ContentHeaderBuf chdr_buf;
	NCIDHeaderBuf *ncid_header = chdr_buf.mutable_ncid_header();
	ncid_header->set_len(_len);
	ncid_header->set_ttl(_ttl);
	ncid_header->set_id(_id);
	ncid_header->set_store_id(_store_id);
	ncid_header->set_signature(_signature);
	ncid_header->set_publisher_name(_publisher_name);
	ncid_header->set_uri(_uri);
	std::string serialized_header;
	chdr_buf.SerializeToString(&serialized_header);
	return serialized_header;
}

bool
NCIDHeader::deserialize(const std::string &buf)
{
	ContentHeaderBuf chdr_buf;
	if(chdr_buf.ParseFromString(buf) == false) {
		return false;
	}
	if(!chdr_buf.has_ncid_header()) {
		return false;
	}
	const NCIDHeaderBuf ncid_hdr = chdr_buf.ncid_header();
	_len = ncid_hdr.len();
	_ttl = ncid_hdr.ttl();
	_id = ncid_hdr.id();
	_store_id = ncid_hdr.store_id();
	_signature = ncid_hdr.signature();
	_publisher_name = ncid_hdr.publisher_name();
	_uri = ncid_hdr.uri();
	return true;
}

bool
NCIDHeader::valid_data(const std::string &data)
{
	// TODO - Do we need to verify that NCID is correct?
	// Maybe a new function to validate that the NCID header is correct
	// Which basically runs the constructor to verify deserialized values

	// Retrieve Publisher name from _uri
	PublisherList *publishers = PublisherList::get_publishers();
	Publisher *publisher = publishers->get(_publisher_name);

	// Call Publisher::isValidSignature()
	return publisher->isValidSignature(_uri, data, _signature);
}

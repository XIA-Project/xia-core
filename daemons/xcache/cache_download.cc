#include "cache_download.h"

void cache_download::set_cid(std::string cid)
{
	_cid = cid;
}

std::string cache_download::cid()
{
	return _cid;
}

void cache_download::set_chdr_len(uint32_t chdr_len)
{
	_chdr_len = chdr_len;
}

uint32_t cache_download::chdr_len()
{
	return _chdr_len;
}

void cache_download::set_chdr_data(char *chdr_data)
{
	_chdr_data = chdr_data;
}

char *cache_download::chdr_data()
{
	return _chdr_data;
}

void cache_download::set_chdr(ContentHeader *chdr)
{
	_chdr = chdr;
}

ContentHeader *cache_download::chdr()
{
	return _chdr;
}

void cache_download::set_data_buf(char *data)
{
	_data = data;
}

char *cache_download::data_buf()
{
	return _data;
}

void cache_download::set_meta(xcache_meta *meta)
{
	_meta = meta;
}

xcache_meta *cache_download::meta()
{
	return _meta;
}

cache_download::cache_download()
{
	_chdr_len = 0;
	_chdr_data = NULL;
	_chdr = NULL;
	_data = NULL;
	_meta = NULL;
}

cache_download::~cache_download()
{
	if(_chdr_data) {
		free(_chdr_data);
	}
	// _chdr, _data, _meta must be explicitly destroy()ed
}

void cache_download::destroy()
{
	if(_chdr_data) {
		free(_chdr_data);
	}
	if(_chdr) {
		delete _chdr;
	}
	if(_data) {
		free(_data);
	}
	if(_meta) {
		delete _meta;
	}
}

#ifndef _XKEYS_HH
#define _XKEYS_HH

#include <string>
#include <exception>
#include <stdexcept>
#include <iostream>

#include "Xsocket.h"
#include "Xkeys.h"

class SIDKey {
	public:
		SIDKey();
		~SIDKey();
		std::string to_string();
	private:
		std::string _sid;
};

SIDKey::SIDKey()
{
	char sid_str[strlen("SID:") + XIA_SHA_DIGEST_STR_LEN];
	if(XmakeNewSID(sid_str, sizeof(sid_str))) {
		throw std::runtime_error("XmakeNewSID failed");
	}
	_sid.assign(sid_str, sizeof(sid_str));
}

SIDKey::~SIDKey()
{
	if(XremoveSID(_sid.c_str())) {
		std::cout << "ERROR remove key manually for " << _sid << std::endl;
	}
}

std::string SIDKey::to_string()
{
	return _sid;
}

#endif // _XKEYS_HH

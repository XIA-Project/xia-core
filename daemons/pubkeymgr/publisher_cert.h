#ifndef _PUBLISHER_CERT_H
#define _PUBLISHER_CERT_H

#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <string>
#include <string.h>
#include <assert.h>
#include <iostream>
#include <Xsecurity.h>
#include <Xsocket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

class PublisherCert{
	public:
		PublisherCert(std::string certpath);
		bool is_valid();
		bool extract_pubkey();
	private:
		bool isRegularFile(std::string filepath);
		X509* load();
		bool check(X509_STORE *store);
		std::string _certpath;
		std::string _cafilepath;
};
#endif // _PUBLISHER_CERT_H

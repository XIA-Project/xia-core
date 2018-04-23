#include <iostream>      // cout, endl
#include <assert.h>
#include <string.h>
#include <Xsocket.h>
#include <Xsecurity.h>
#include <xcache.h>
#include "publisher.h"
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>    // stat
#include <sys/stat.h>     // stat
#include <unistd.h>       // stat

/*!
 * @brief An Object representing a Publisher named on instantiation
 */
Publisher::Publisher(std::string name)
{
	_name = name;
	_cert_dag = nullptr;

}

Publisher::~Publisher()
{
	if(_cert_dag) {
		delete _cert_dag;
	}
}

/*!
 * @brief The publisher's name
 */
std::string Publisher::name()
{
	return _name;
}

/*!
 * @brief Build Content URI for the requested content
 */
std::string Publisher::content_URI(std::string content_name)
{
	return name() + "/" + content_name;
}

/*!
 * @brief Publisher's public key
 *
 * The key manager is contacted via a unix domain socket unless the
 * public key is already known. The key manager provides the public
 * key for this publisher along with the certificate address.
 *
 * The certificate address is used as the base address for named chunks
 * published by this publisher.
 */
std::string Publisher::pubkey()
{
	if (_pubkey.size() == 0) {
		// Connect to keymanager and get publisher's key and certificate addr
		char key[MAX_PUBKEY_SIZE];
		char cert_dag[XIA_MAX_DAG_STR_SIZE];
		size_t keylen = sizeof(key);
		size_t cert_daglen = sizeof(cert_dag);

		if (XgetPublisherCreds(_name.c_str(), key, &keylen,
					cert_dag, &cert_daglen)) {
			std::cout << "Publisher::pubkey not found" << std::endl;
			return "";
		}
		if(cert_daglen != 0) {
			_cert_dag = new Graph(std::string(cert_dag, cert_daglen));
		}
		_pubkey = std::string(key, keylen);
	}
	return _pubkey;
}

/*!
 * @brief calculate NCID for the given content name
 *
 * @returns NCID for given content name
 */
std::string Publisher::ncid(std::string content_name)
{
	std::string URI = content_URI(content_name);
	std::string pubkeystr = pubkey();
	if(pubkeystr.size() == 0) {
		std::cout << "Publisher::ncid: ERROR return empty string" << std::endl;
		return "";
	}
	std::string ncid_data = URI + pubkeystr;
	char ncidhex[XIA_SHA_DIGEST_STR_LEN];
	int ncidlen = XIA_SHA_DIGEST_STR_LEN;
	xs_getSHA1HexDigest((const unsigned char *)ncid_data.c_str(),
			ncid_data.size(), ncidhex, ncidlen);
	assert(strlen(ncidhex) == XIA_SHA_DIGEST_STR_LEN - 1);
	std::string ncidstr(ncidhex);
	return "NCID:" + ncidstr;

}

/*!
 * @brief get a DAG for requested content name
 *
 * Convert a content name to the corresponding DAG. This involves
 * a lot of complex set of steps:
 * 1. Download the Publisher's certificate, if necessary.
 * 2. Verifying the certificate against the CA root certiifcate
 * 3. Extracting the public ke from the Publisher Certificate
 * 4. Calculating the NCID from pubkey and content_name
 * 5. Creating DAG by substituting intent CID in Publisher cert with NCID
 *
 * @returns DAG for retrieving the NCID on success
 * @returns empty string on failure
 */
std::string Publisher::ncid_dag(std::string content_name)
{
	// The NCID calculation fetches the public key and its cert dag
	std::string ncidstr = ncid(content_name);
	if(ncidstr.size() == 0) {
		std::cout << "Publisher::ncid_dag() ncid calc failed" << std::endl;
		return "";
	}
	Node ncid_node(ncidstr);

	// Ensure that the certificate dag was fetched during ncid calculation
	if(_cert_dag == nullptr) {
		std::cout << "Publisher::ncid_dag() invalid cert dag" << std::endl;
		return "";
	}
	// Replace Publisher certificate CID in address with NCID of content
	// ASSUMPTION: Certificate is in the same xcache as content being requested
	Graph dag(*_cert_dag);
	dag.replace_final_intent(ncid_node);
	return dag.dag_string();
}

/*!
 * @brief Sign the given content to associate it with given content_URI
 *
 * @returns signature for the content, if successful
 * @returns 0 on success -1 on failure
 */
int Publisher::sign(std::string content_URI,
		const std::string &content,
		std::string &signature)
{
	char sig_buf[MAX_SIGNATURE_SIZE];
	uint16_t siglen = MAX_SIGNATURE_SIZE;

	// Sign (Content URI + Content)
	std::string data = content_URI + content;

	// Get the digest of data
	uint8_t digest[SHA_DIGEST_LENGTH];
	xs_getSHA1Hash((const unsigned char *)data.c_str(), data.size(),
			digest, sizeof(digest));

	// Now request Key Manager to find key and sign
	if(XcacheSignContent(name().c_str(), (const char *)digest, sizeof(digest),
				sig_buf, &siglen)) {
		printf("Publisher::sign() failed signing provided content\n");
		return -1;
	}
	signature.assign(sig_buf, siglen);
	return 0;
}

/*!
 * @brief Verify the signature
 *
 * @returns true on success, false on failure.
 */
bool Publisher::isValidSignature(std::string content_URI,
		const std::string &content,
		const std::string &signature)
{
	// The publisher signed (Content URI + Content)
	std::string data = content_URI + content;

	// Get the digest of data
	uint8_t digest[SHA_DIGEST_LENGTH];
	xs_getSHA1Hash((const unsigned char *)data.c_str(), data.size(),
			digest, sizeof(digest));

	if(XcacheVerifyContent(name().c_str(), (const char *)digest, sizeof(digest),
				signature.c_str(), signature.size())) {
		printf("Publisher::isValidSignature() invalid signature\n");
		return false;
	}
	return true;
}

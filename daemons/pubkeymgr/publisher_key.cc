// XIA Headers
#include "publisher_key.h"
#include "publisher_cert.h"
#include "xcache_cmd.pb.h"

// System headers
#include <stdlib.h>
#include <stdio.h>
#include <iostream>      // cout, endl
#include <assert.h>
#include <string.h>
#include <sys/types.h>    // stat
#include <sys/stat.h>     // stat
#include <unistd.h>       // stat

/*!
 * @brief An Object representing a PublisherKey named on instantiation
 */
PublisherKey::PublisherKey(std::string name)
{
	char *keydir = (char *)malloc(MAX_KEYDIR_PATH_LEN);
	assert(keydir != NULL);

	std::string publisher_dir = "/publishers/" + name;
	if(XrootDir((char *)keydir, MAX_KEYDIR_PATH_LEN) == NULL) {
		printf("ERROR: Unable to get XIA root directory name");
		assert(-1);
	}

	// Protect from buffer overflow attacks on strcat
	assert(strlen(keydir) + publisher_dir.size() < MAX_KEYDIR_PATH_LEN);
	_keydir = strcat(keydir, publisher_dir.c_str());

	_name = name;
	_cert_dag = nullptr;
}

PublisherKey::~PublisherKey()
{
	if(_keydir!= NULL) {
		free((void *)_keydir);
	}
}

/*!
 * @brief The publisher's name
 */
std::string PublisherKey::name()
{
	return _name;
}

/*!
 * @brief PublisherKey's public key
 */
std::string PublisherKey::pubkey()
{
	char pubkeybuf[MAX_PUBKEY_SIZE];
	uint16_t pubkeylen = MAX_PUBKEY_SIZE;
	memset(pubkeybuf, 0, pubkeylen);

	// Pubkey should be in a file in the credential directory
	std::string pubkeypath = keydir() + "/" + name() + ".pub";

	// If the pubkey is not available, try getting it
	if(!pubkey_present(pubkeypath)) {
		if(fetch_pubkey() == false) {
			return "";
		}
	}
	// Find the pubkey file in the PublisherKey's credential directory
	if(xs_readPubkeyFile(pubkeypath.c_str(), pubkeybuf, &pubkeylen)) {
		printf("PublisherKey::pubkey() cannot read key from %s\n",
				pubkeypath.c_str());
		return "";
	}
	std::string pubkeystr(pubkeybuf, pubkeylen);
	return pubkeystr;
}

/*!
 * @brief Publisher's certificate address (DAG)
 *
 * The address where we downloaded the publisher's certificate from
 * 
 * We currently use this address as the base for all chunks published
 * by the said publisher. This is just a shortcut until we have another
 * way for the publisher to say where the chunks published by it would
 * be placed.
 *
 * Returns certificate dag as a string
 */
std::string PublisherKey::cert_dag_str()
{
	if(fetch_cert_dag() == false) {
		return "";
	}
	// A valid cert dag should be in _cert_dag
	return _cert_dag->dag_string();
}

bool PublisherKey::keydir_present()
{
	struct stat statbuf;

	if(stat(_keydir, &statbuf)) {
		return false;
	}
	if(!S_ISDIR(statbuf.st_mode)) {
		std::cout << "ERROR: Creds directory invalid" << std::endl;
		return false;
	}
	return true;
}

/*!
 * @brief Create _keydir if not present
 *
 * @returns true on success or existing, false on failure
 */
bool PublisherKey::ensure_keydir_exists()
{
	if(!keydir_present()) {
		if(mkdir(_keydir, S_IRWXU | S_IRWXG | S_IRWXO)) {
			std::cout << "ERROR creating creds dir " << _keydir << std::endl;
			return false;
		}
		if(chmod(_keydir, S_IRWXU | S_IRWXG | S_IRWXO)) {
			std::cout << "ERROR setting permissions on dir" << std::endl;
			return false;
		}
	}
	return true;
}

/*!
 * @brief Check if the public key is in provided path
 *
 * For now, we just check to see if the path contains a file
 * TODO: make sure that a valid public key is available in file
 */
bool PublisherKey::pubkey_present(std::string path)
{
	struct stat statbuf;

	if(stat(path.c_str(), &statbuf)) {
		return false;
	}
	if(!S_ISREG(statbuf.st_mode)) {
		std::cout << "ERROR: pubkey path not a regular file" << std::endl;
		return false;
	}
	return true;
}

/*!
 * @brief Certificate name as registered with nameservice
 *
 * Currently we register <publisher_name>.publisher.cert.xia and the
 * certificate DAG with the nameservice. In future, there could be a
 * separate service or distributed source for finding certificate DAGs.
 */
std::string PublisherKey::cert_name()
{
	assert(_name.size() > 0);
	return _name + ".publisher.cert.xia";
}

/*!
 * @brief Query the nameservice for the Certificate DAG address
 *
 * We use the DAG of the PublisherKey's certificate to build the DAG for
 * NCIDs published by the PublisherKey. This is a hacky but gives us a
 * DAG for somewhere we know the PublisherKey is able to publish stuff.
 * In the real world, the PublisherKey would probably make DAGs available
 * for clients to use to get NCID content.
 *
 * This function populates _cert_dag.
 *
 * @returns true if the DAG for the PublisherKey's certificate was found
 * @returns false if the DAG was not found
 */
bool PublisherKey::fetch_cert_dag()
{
	sockaddr_x addr;
	socklen_t addrlen;
	addrlen = sizeof(addr);

	// Quickly return success if _cert_dag is known already
	if(_cert_dag) {
		return true;
	}

	// Otherwise, fetch the certificate address from nameservice
	std::string cert_url = cert_name();

	if(XgetDAGbyName(cert_url.c_str(), &addr, &addrlen)) {
		std::cout << "PublisherKey::fetch_cert_dag: "
			<< "could not be found" << std::endl;
		_cert_dag = NULL;
		return false;
	}

	_cert_dag = std::unique_ptr<Graph>(new Graph(&addr));
	return true;
}

/*!
 * @brief Fetch the public key to the provided path
 *
 * Query the nameserver to see if a certificate for this PublisherKey is
 * available for download. If one is available, we download the certificate
 * and extract the public key from it. Then we verify it against the
 * CA cert that we already have. If all checks out, store public key
 * for publisher and return success.
 */
bool PublisherKey::fetch_pubkey()
{
	sockaddr_x addr;
	int state = 0;
	bool retval = false;
	xcache_cmd resp;
	xcache_cmd cmd;
	int certlen;
	void *cert;
	std::string certpath;
	PublisherCert *pubcert;
	int flags = XCF_BLOCK;

	// Store certificate DAG for building NCID DAGs in future
	if(fetch_cert_dag() == false) {
		std::cout << "PublisherKey::fetch_pubkey: " <<
			"ERROR fetching PublisherKey cert addr" << std::endl;
		return false;
	}

	// xcache operates on sockaddr_x, so fill one in
	_cert_dag->fill_sockaddr(&addr);

	// Fetch PublisherKey cert
	if(XcacheHandleInit(&_h)) {
		std::cout << "PublisherKey: ERROR connecting to Xcache" << std::endl;
		assert(false);
	}
	certlen = XfetchChunk(&_h, &cert, flags, &addr, sizeof(addr));
	XcacheHandleDestroy(&_h);
	if (certlen < 0) {
		std::cout << "ERROR Pubkey not found for " << name() << std::endl;
		goto fetch_pubkey_done;
	}

	if(cert == NULL) {
		std::cout << "ERROR getting memory for cert" << std::endl;
		goto fetch_pubkey_done;
	}
	state = 1;

	// Write PublisherKey cert to disk
	// FIXME: Write to a staging area until verification
	if(store_publisher_cert(cert, (size_t)certlen, certpath) == false) {
		std::cout << "Unable to store cert on disk" << std::endl;
		goto fetch_pubkey_done;
	}

	// Verify PublisherKey cert against CA cert
	pubcert = new PublisherCert(certpath);
	state = 2;

	if(!pubcert->is_valid()) {
		std::cout << "ERROR invalid publisher cert " << certpath << std::endl;
		goto fetch_pubkey_done;
	}

	// Extract Public key from PublisherKey cert
	if(!pubcert->extract_pubkey()) {
		std::cout << "ERROR extracting pubkey" << certpath << std::endl;
		goto fetch_pubkey_done;
	}
	retval = true;

	// Store valid PublisherKey cert and valid Pubkey in _keydir
	// TODO Currently storing before verification. Verify before store.
fetch_pubkey_done:
	switch(state) {
		case 2:
			delete pubcert;
			[[gnu::fallthrough]];
		case 1:
			free(cert);
			[[gnu::fallthrough]];
	};

	return retval;
}

/*!
 * @brief Write publisher certificate from memory to disk
 *
 * Provided certificate in cert is written to disk based on the
 * publisher's name. Correctness of certificate contents is not
 * checked at this time and must be ensured by caller.
 *
 * @returns true on success, false on failure
 * @returns path to certificate file if successful in storing
 */
bool PublisherKey::store_publisher_cert(void *cert, size_t len,
	std::string &certfilepath)
{
	FILE *certfile;
	int state = 0;
	bool retval = false;
	std::string certpath(_keydir);

	// Create directory to store PublisherKey credentials
	if(!ensure_keydir_exists()) {
		std::cout << "ERROR creds directory unavailable" << std::endl;
		goto store_publisher_cert_done;
	}

	// Now write certificate to disk
	certpath += "/" + _name + ".cert";
	certfile = fopen(certpath.c_str(), "w");
	state = 1;

	if(fwrite(cert, 1, len, certfile) != len) {
		std::cout << "ERROR writing certificate to disk" << std::endl;
		goto store_publisher_cert_done;
	}
	certfilepath = certpath;

	// Certificate successfully written to disk
	retval = true;

store_publisher_cert_done:
	switch(state) {
		case 1: if(fclose(certfile)) {
					std::cout << "Failed closing cert file" << std::endl;
				}
	}

	return retval;
}

/*!
 * @brief Directory containing all PublisherKey security credentials
 */
std::string PublisherKey::keydir()
{
	return _keydir;
}

std::string PublisherKey::privfilepath()
{
	std::string privkeypath = keydir() + "/" + name() + ".priv";
	return privkeypath;
}

std::string PublisherKey::pubfilepath()
{
	// Replace the last 4 chars in privfilepath: "priv" with "pub"
	std::string pubkeypath = privfilepath();
	pubkeypath.replace(pubkeypath.size()-4, 4, "pub");
	return pubkeypath;
}

int PublisherKey::sign(std::string &digest, std::string &signature)
{
	std::string privkeyfile = privfilepath();

	unsigned char sigbuf[MAX_SIGNATURE_SIZE];
	uint16_t sigbuflen = sizeof(sigbuf);

	if (xs_signDigestWithKey(privkeyfile.c_str(),
				(uint8_t *)digest.c_str(), digest.size(),
				sigbuf, &sigbuflen)) {
		std::cout << "PublisherKey: failed signing" << std::endl;
		return -1;
	}
	signature.assign((const char *)sigbuf, (size_t)sigbuflen);
	return 0;
}

/**
 * @brief check the signature for the provided digest
 *
 * @returns 0 if the signature matches the data
 * @returns -1 if the signature doesn't match or another error occurs
 */
int PublisherKey::checkSignature(std::string &digest, std::string &signature)
{
	std::string pubkeyfile = pubfilepath();

	if(xs_isValidDigestSignature(pubkeyfile.c_str(),
				(const unsigned char *)digest.c_str(), digest.size(),
				(unsigned char *)signature.c_str(), signature.size()) != 1) {
		std::cout << "ERROR invalid signature" << std::endl;
		return -1;
	}
	return 0;
}

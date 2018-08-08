// -*- c-basic-offset: 4; related-file-name: "../include/click/xiasecurity.hh" -*-
#ifdef CLICK_USERLEVEL
#include <click/config.h>
#include <click/xiasecurity.hh>
#include <click/xiautil.hh>
#else
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "Xsecurity.h"
#include "Xsocket.h"
#endif // CLICK_USERLEVEL
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <strings.h> // bzero

/*!
 * @brief print debug output in click as well as user-space
 *
 * The XIA Stack is written in Click and has access to click_chatter()
 * functions for output. However, the same functionality is not available
 * to user space applications. This file is compiled both for use in
 * Click as well as user-space so it needs its own output functionality.
 *
 * @param fmt formatted string similar to printf
 *
 * @returns void
 */
void xs_chatter(const char *fmt, ...)
{
	va_list val;
	va_start(val, fmt);
	vfprintf(stderr, fmt, val);
	fprintf(stderr, "\n");
	va_end(val);
}

/*!
 * @brief XIASecurityBuffer serialization/deserialization class constructor
 *
 * The user can provide an estimated size to initialize the internal buffer
 * in the constructor. If the estimate is too low, additional space will
 * be allocated automatically with a slight performance penalty. If the
 * size estimate is too high, memory will be wasted.
 *
 * @param initSize estimated initial size of buffer
 */
XIASecurityBuffer::XIASecurityBuffer(int initSize)
{
	numEntriesSize = sizeof(uint16_t);
	initialSize = numEntriesSize + initSize; // numEntries + requested size
	initialized = false;
	numEntriesPtr = NULL;
	dataPtr = NULL;
	remainingSpace = 0;
	_buffer = NULL;
	nextPack = NULL;
	nextUnpack = NULL;
}

/*!
 * @brief Construct XIASecurityBuffer from a binary buffer
 *
 * A serialized representation of XIASecurityBuffer can be provided to
 * this constructor to deserialize the memory contents.
 *
 * @param buf buffer containing a valid serialized XIASecurityBuffer
 * @param len length of buf
 */
XIASecurityBuffer::XIASecurityBuffer(const char *buf, uint16_t len)
{
	numEntriesSize = sizeof(uint16_t);
	// Copy the serialized buffer
	_buffer = (char *)calloc(len, 1);
	if(_buffer == NULL) {
		xs_chatter("XIASecurityBuffer ERROR: allocating %d bytes", len);
		throw std::bad_alloc();
	}
	memcpy(_buffer, buf, len);
	// Update pointers
	nextPack = _buffer+len;
	dataPtr = _buffer + numEntriesSize;
	numEntriesPtr = (uint16_t *)_buffer;
	nextUnpack = dataPtr;
	// Update state variables
	initialSize = len;
	remainingSpace = 0;
	initialized = true;
}

/*!
 * @brief XIASecurityBuffer destructor frees internally allocated memory
 *
 * Free up the internally allocated memory for this security buffer
 */
XIASecurityBuffer::~XIASecurityBuffer()
{
	if(_buffer) {
		free(_buffer);
	}
}

/*!
 * @brief allocate the initial memory for use by the object
 *
 * Initialize storage space for this object to store data. We store
 * the number of entries in the first two bytes of the buffer itself.
 * This is followed by size and corresponding data for each entry. So
 * when the user requests a serialized buffer, we can simply return
 * the internal _buffer.
 *
 * @returns true on success
 * @returns false on failure to initialize the storage space
 */
bool XIASecurityBuffer::initialize()
{
	_buffer = (char *)calloc(initialSize, 1);
	if(_buffer == NULL) {
		return false;
	}
	numEntriesPtr = (uint16_t *)_buffer;
	*numEntriesPtr = 0;
	dataPtr = _buffer + numEntriesSize;
	remainingSpace = initialSize - numEntriesSize;
	nextPack = dataPtr;
	initialized = true;
	return true;
}

/*!
 * @brief Extend buffer to store the length plus length-bytes of data
 *
 * Check if the requested bytes can be stored in the internal buffer.
 * If not, extend it automatically to be large enough to store, at
 * least, the requested number of bytes.
 *
 * @param length number of bytes we intend to store
 *
 * @returns true on success in extending the buffer
 * @returns false on failure to extend the buffer
 */
bool XIASecurityBuffer::extend(uint32_t length)
{
	uint16_t total_bytes = sizeof(uint16_t) + length;
	// Extend the buffer if needed
	while(remainingSpace < total_bytes) {
		int currentDataSize = nextPack - dataPtr;
		int newSize = numEntriesSize + (2*currentDataSize);
		int nextUnpackOffset = 0;
		if(nextUnpack != NULL) {
			nextUnpackOffset = nextUnpack - _buffer;
		}
		char *newbuffer = (char *)realloc(_buffer, newSize);
		if(newbuffer == NULL) {
			xs_chatter("XIASecurityBuffer::extend: failed extending size beyond %d", currentDataSize);
			return false;
		}
		numEntriesPtr = (uint16_t *)newbuffer;
		dataPtr = newbuffer + numEntriesSize;
		remainingSpace = newSize - (currentDataSize + numEntriesSize);
		nextPack = newbuffer + (currentDataSize + numEntriesSize);
		if(nextUnpack != NULL) {
			nextUnpack = newbuffer + nextUnpackOffset;
		}
		_buffer = newbuffer;
		//bzero(_buffer+currentSize, currentSize);
	}
	return true;
}

/*!
 * @brief Get a serialized buffer containing all the data stored
 *
 * Since all the data is already stored serialized, we simply return
 * the internal buffer when the user requests serialized data with
 * this function call.
 *
 * @returns buffer holding serialized data
 */
char *XIASecurityBuffer::get_buffer()
{
	if(initialized) {
		return _buffer;
	} else {
		return NULL;
	}
}

/*!
 * @brief Size of the serialized XIASecurityBuffer
 *
 * Used along with get_buffer() this function returns the serialized
 * buffer's size.
 *
 * @returns number of bytes in serialized buffer at this time
 * @returns 0 if the buffer is uninitialized or contains no data
 */
uint16_t XIASecurityBuffer::size()
{
	if(initialized) {
		return nextPack - _buffer;
	} else {
		return 0;
	}
}

/*!
 * @brief Find the number of entries in the buffer
 *
 * An XIASecurityBuffer is just a collection of [size, data] entries.
 * This function provides the number of such entries in the current
 * buffer.
 *
 * @return number of data objects in the current buffer
 */
uint16_t XIASecurityBuffer::get_numEntries()
{
	return *numEntriesPtr;
}

/*!
 * @brief pack the provided data as an entry in the XIASecurityBuffer
 *
 * @param data a buffer containing the data to be packed
 * @length number of bytes in the buffer
 *
 * @returns true on success
 * @returns false on failure
 */
bool XIASecurityBuffer::pack(const char *data, uint16_t length)
{
	if(!initialized) {
		if(initialize() == false) {
			xs_chatter("XIASecurityBuffer::pack: failed initializing memory for data");
			return false;
		}
	}
	if(extend(length) == false) {
		xs_chatter("XIASecurityBuffer::pack: failed extending memory for additional data");
		return false;
	}

	if(nextUnpack == NULL) {
		nextUnpack = nextPack;
	}
	memcpy(nextPack, &length, sizeof(length));
	nextPack += sizeof(length);
	memcpy(nextPack, data, length);
	nextPack += length;
	remainingSpace -= (sizeof(uint16_t) + length);
	*numEntriesPtr = *numEntriesPtr + 1;
	return true;
}

/*!
 * @brief pack the provided null terminated string into the XIASecurityBuffer
 *
 * A convenience function to pack a null-terminated string as a data entry
 * into the XIASecurityBuffer.
 *
 * @param data a null-terminated string
 *
 * @returns true on successful packing of data
 * @returns false if the packing fails
 */
bool XIASecurityBuffer::pack(const char *data)
{
	uint16_t len = strlen(data);
	// TODO: Add asserts to validate length here
	return pack(data, len);
}

/*!
 * @brief sign and include signature in the XIASecurityBuffer
 *
 * The user provides an XIASecurityBuffer containing all the data to be signed.
 * This function puts the user's XIASecurityBuffer as an entry in this buffer
 * and then includes the signature and the corresponding public key in this
 * buffer. The signing is only possible if the key files for the requested
 * XIA XID are available for signing.
 *
 * @param payloadbuf another XIASecurityBuffer whose contents will be signed
 * @param xid_str the XIA XID signing this message. Keys must be present.
 *
 * @returns true if we are able to sign and pack the requested buffer
 * @returns false if the signing or packing fails
 */
bool XIASecurityBuffer::sign_and_pack(XIASecurityBuffer &payloadbuf, const std::string &xid_str)
{

    unsigned char *payload = (unsigned char *)payloadbuf.get_buffer();
    uint16_t payloadlen = payloadbuf.size();

    // Sign the rv_control message
    uint8_t signature[MAX_SIGNATURE_SIZE];
    uint16_t siglen = MAX_SIGNATURE_SIZE;
    if(xs_sign(xid_str.c_str(), payload, payloadlen, signature, &siglen)) {
        xs_chatter("xs_chatter: Signing rv_control message");
        return false;
    }

    // Extract the pubkey for inclusion in rv_control message
    char pubkey[MAX_PUBKEY_SIZE];
    uint16_t pubkeylen = MAX_PUBKEY_SIZE;
    if(xs_getPubkey(xid_str.c_str(), pubkey, &pubkeylen)) {
        xs_chatter("xs_chatter: Pubkey not found:%s", xid_str.c_str());
        return false;
    }

    // Fill in the rv_control message
    if(!pack((const char *) payload, payloadlen)) {
        xs_chatter("Failed packing rv_control payload into rv_control message");
        return false;
    }
    if(!pack((const char *) signature, siglen)) {
        xs_chatter("Failed packing signature into rv_control message");
        return false;
    }
    if(!pack((const char *) pubkey, pubkeylen)) {
        xs_chatter("Failed packing pubkey into rv_control message");
        return false;
    }

    return true;
}

/*!
 * @brief Unpack items from buffer, one per call
 *
 * Unpack data entries from the buffer one at a time.
 * Typical usage: while(unpack(buf, &len)) ... do something
 *
 * @param data a buffer in which the data entry will be unpacked
 * @param length size of the user provided data buffer
 *
 * @returns length of actual data entry in the length pointer
 * @returns true on successfully unpacking an entry
 * @returns false on failure to unpack an entry
 */
bool XIASecurityBuffer::unpack(char *data, uint16_t *length)
{
	// zero out the buffer so user won't be burned if they unpack a string
	bzero(data, *length);

	// If we go past the nextPack pointer, no more entries to unpack
	if(nextUnpack >= nextPack) {
		xs_chatter("XIASecurityBuffer::unpack: ERROR no entries to unpack");
		return false;
	}

	if(*length < peekUnpackLength()) {
		xs_chatter("XIASecurityBuffer::unpack: ERROR insufficient buffer space:%d, expected:%d.", *length, peekUnpackLength());
		return false;
	}
	*length = peekUnpackLength();
	nextUnpack += sizeof(uint16_t);
	memcpy(data, nextUnpack, *length);
	nextUnpack += *length;
	return true;
}

int XIASecurityBuffer::peekUnpackLength()
{
	if(nextUnpack >= nextPack) {
		xs_chatter("XIASecurityBuffer::peekUnpackLength: ERROR reading past end of buffer");
		return -1;
	}
	uint16_t length;
	memcpy(&length, nextUnpack, sizeof(uint16_t));
	return (int)length;
}

/*!
 * @brief find the key directory where the key-pairs for signing are stored
 *
 * This function is just a wrapper around the two different APIs for getting
 * the key directory in Click and user-facing API
 *
 * @returns a string containing the key directory path
 */
static const char *get_keydir()
{
    static const char *keydir = NULL;
    if(keydir == NULL) {
        keydir = (const char *)calloc(MAX_KEYDIR_PATH_LEN, 1);
        if(keydir == NULL) {
            return NULL;
        }
#ifdef CLICK_USERLEVEL
        if(xiaRootDir((char *)keydir, MAX_KEYDIR_PATH_LEN) == NULL) {
            return NULL;
        }
#else
		if(XrootDir((char *)keydir, MAX_KEYDIR_PATH_LEN) == NULL) {
			return NULL;
		}
#endif //CLICK_USERLEVEL
        strcat((char *)keydir, "/");
        strcat((char *)keydir, XIA_KEYDIR);
    }
    return keydir;
}

/*!
 * @brief Generate SHA1 hash of a given buffer
 *
 * Given a buffer containing user data, generate its SHA-1 hash
 *
 * @param data user data buffer
 * @param data_len size of the data buffer
 * @param digest a buffer where the generated SHA-1 hash will be stored
 * @praam digest_len length of the digest buffer. Must be SHA_DIGEST_LENGTH.
 *
 * @returns void
 */
void xs_getSHA1Hash(const unsigned char *data, int data_len, uint8_t* digest, int digest_len)
{
	assert(digest_len == SHA_DIGEST_LENGTH);
	(void) digest_len;
	SHA1(data, data_len, digest);
}

/*!
 * @brief Generate SHA1 hash and return a hex string representing it
 *
 * Given a buffer containing user data, generate its SHA-1 hash
 * and convert it into a hex string that is returned to the caller.
 *
 * @param data user data buffer
 * @param data_len size of the data buffer
 * @param hex_string buffer to hold the hex string being returned
 * @param hex_string_len length of hex_string buffer
 *
 * @returns 0 on success
 * @returns negative value on failure
 */
void xs_getSHA1HexDigest(const unsigned char * data, int data_len,
		char *hex_string, int hex_string_len)
{
	uint8_t digest[SHA_DIGEST_LENGTH];
	xs_getSHA1Hash(data, data_len, digest, sizeof(digest));
	xs_hexDigest(digest, sizeof(digest), hex_string, hex_string_len);
}

/*!
 * @brief Convert a SHA1 digest to a hex string
 *
 * Convert the binary SHA-1 hash into a hex string
 *
 * Note: this function does not return the hex_string's length because it
 * is already null-terminated and the user can call strlen() on it.
 *
 * @param digest a buffer containing the SHA-1 hash to be converted
 * @param digest_len length of the SHA-1 buffer
 * @param hex_string the user buffer to hold the converted hex string
 * @param hex_string_len length of the hex_string buffer
 *
 * @returns void
 */
void xs_hexDigest(uint8_t* digest, int digest_len, char* hex_string, int hex_string_len)
{
    int i;
	assert(digest != NULL);
	assert(hex_string != NULL);
	assert(hex_string_len == XIA_SHA_DIGEST_STR_LEN);
	assert(digest_len == SHA_DIGEST_LENGTH);
    for(i=0;i<digest_len;i++) {
        sprintf(&hex_string[2*i], "%02x", (unsigned int)digest[i]);
    }
    hex_string[hex_string_len-1] = '\0';
}

/*!
 * @brief generate a hash of the given public key
 *
 * Given a public key, generate it's SHA-1 hash.
 *
 * @param pubkey a string representation of the public key to be hashed
 * @param digest user provided buffer to hold the SHA-1 hash
 * @param digest_len length of the digest buffer
 *
 * @returns 0 on success
 * @returns -1 on failure
 */
int xs_getPubkeyHash(char *pubkey, uint8_t *digest, int digest_len)
{
	int i, j;
	char *pubkeystr;
	char pubkeyheader[] = "-----BEGIN PUBLIC KEY-----\n";
	char pubkeyfooter[] = "-----END PUBLIC KEY-----\n";
	int pubkeyheaderlen = sizeof pubkeyheader - 1;
	char *pubkeystart = strstr(pubkey, pubkeyheader);
	pubkeystart += pubkeyheaderlen;
	char *pubkeyend = strstr(pubkey, pubkeyfooter);

	assert(digest != NULL);
	assert(digest_len == SHA_DIGEST_LENGTH);

	// Strip header, footer and newlines
	pubkeystr = (char *)calloc(pubkeyend - pubkeystart, 1);
	if(pubkeystr == NULL) {
		xs_chatter("xs_getPubkeyHash: ERROR allocating memory for public key");
		return -1;
	}
	for(i=0,j=0; i<pubkeyend-pubkeystart; i++) {
		if(pubkeystart[i] != '\n') {
			pubkeystr[j++] = pubkeystart[i];
		}
	}
	xs_chatter("xs_getPubkeyHash: Stripped pubkey: %s", pubkeystr);
	xs_getSHA1Hash((const unsigned char *)pubkeystr, strlen(pubkeystr), digest, digest_len);

	free(pubkeystr);
	return 0;
}

/*!
 * @brief Verify signature using public key read from file for given xid
 *
 * NOTE: Public key must be available from local file
 *
 * @param data data to be verified
 * @param datalen length of the data buffer
 * @param signature a buffer holding the signature to be verified against
 * @param siglen the size of the signature buffer
 * @param xid the XIA XID to be used for verification
 *
 * @returns 1 if the signature is valid
 * @returns 0 if the signature does not match the data
 */
int xs_isValidSignature(const unsigned char *data, size_t datalen, unsigned char *signature, unsigned int siglen, const char *xid)
{
	char pem_pub[MAX_PUBKEY_SIZE];
	uint16_t pem_pub_len = MAX_PUBKEY_SIZE;

	// Get the public key for given xid
	if(xs_getPubkey(xid, pem_pub, &pem_pub_len)) {
		printf("xs_isValidSignature: ERROR reading pubkey\n");
		return 0;
	}

	return xs_isValidSignature(data, datalen, signature, siglen, pem_pub, pem_pub_len);
}

/*!
 * @brief Verify signature using public key in memory
 *
 * Use this function at a recipient that just received a signed
 * XIASecurityBuffer.
 *
 * NOTE: The public key is provided in a buffer
 *
 * @param data a buffer containing the data to be verified
 * @param datalen length of the data buffer
 * @param signature a buffer holding the signature to be verified against
 * @param siglen the size of the signature buffer
 * @param pem_pub a buffer holding the public key
 * @param length of the pem_pub buffer length
 *
 * @returns 1 if the signature matches data, verified against pem_pub
 * @returns 0 if signature does not match data
 */
int xs_isValidSignature(const unsigned char *data, size_t datalen, unsigned char *signature, unsigned int siglen, char *pem_pub, int pem_pub_len)
{
	int sig_verified;

	BIO *bio_pub = BIO_new(BIO_s_mem());
	BIO_write(bio_pub, pem_pub, pem_pub_len);
	RSA *rsa = PEM_read_bio_RSA_PUBKEY(bio_pub, NULL, NULL, NULL);
	if(!rsa) {
		return 0;
	}
	// Calculate SHA1 hash of given data
	uint8_t digest[SHA_DIGEST_LENGTH];
	xs_getSHA1Hash(data, datalen, digest, sizeof digest);
	// Verify signature against SHA1 hash of data using given public key
	sig_verified = RSA_verify(NID_sha1, digest, sizeof digest, signature, siglen, rsa);
	RSA_free(rsa);
	return sig_verified;
}

/*!
 * @brief Verify signature using public key read from given file
 *
 * @param pubfilepath path of file holding the public key
 * @param data to be verified
 * @param datalen length of data buffer
 * @param signature buffer holding the signature to be verified against
 * @param siglen length of the signature buffer
 *
 * @returns 1 if the signature is valid
 * @returns 0 if the signature does not match the data
 */
int xs_isValidSignature(const char *pubfilepath,
		const unsigned char *data, size_t datalen,
		unsigned char *signature, unsigned int siglen)
{
	char pem_pub[MAX_PUBKEY_SIZE];
	uint16_t pem_pub_len = MAX_PUBKEY_SIZE;
	if(xs_readPubkeyFile(pubfilepath, pem_pub, &pem_pub_len)) {
		printf("xs_isValidSignature: ERROR reading pubkey\n");
		return 0;
	}

	return xs_isValidSignature(data, datalen, signature, siglen,
			pem_pub, pem_pub_len);
}

/*!
 * @brief Verify signature using public key against digest of data
 *
 * @param pubfilepath path of the file holding the public key
 * @param digest the sha1 digest of data to be verified
 * @param digestlen length of digest
 * @param signature buffer holding the signature to be verified against
 * @param siglen length of the signature buffer
 *
 * @returns 1 if the signature is valid
 * @returns 0 if the signature does not match the data digest
 */
int xs_isValidDigestSignature(const char *pubfilepath,
		const unsigned char *digest, size_t digestlen,
		unsigned char *signature, unsigned int siglen)
{
	int sig_verified;

	char pem_pub[MAX_PUBKEY_SIZE];
	uint16_t pem_pub_len = MAX_PUBKEY_SIZE;
	if(xs_readPubkeyFile(pubfilepath, pem_pub, &pem_pub_len)) {
		printf("xs_isValidSignature: ERROR reading pubkey\n");
		return 0;
	}

	BIO *bio_pub = BIO_new(BIO_s_mem());
	BIO_write(bio_pub, pem_pub, pem_pub_len);
	RSA *rsa = PEM_read_bio_RSA_PUBKEY(bio_pub, NULL, NULL, NULL);
	if(!rsa) {
		return 0;
	}
	// Verify signature against SHA1 hash of data using given public key
	sig_verified = RSA_verify(NID_sha1, digest, digestlen,
			signature, siglen, rsa);
	RSA_free(rsa);
	return sig_verified;
}

/*!
 * @brief retrieve XID in hex string form from an XID given as a string
 *
 * Simply strips out the XID: string and returns the hex string. This
 * can be used to look for the corresponding key-pair files.
 *
 * @param xid the XID whose hex ID string should be found
 *
 * @returns the hex string corresponding to the XID
 */
const char *xs_XIDHash(const char *xid)
{
	return strchr((char *)xid, ':') + 1;
}

/*!
 * @brief Sign data using private key of the given xid
 *
 * Sign the given data using the private key corresponding to the given
 * XID. This can only be done if the private key is available locally
 * and is accessible to the calling application.
 *
 * @param xid the XID whose private key will be used for signing
 * @param data the data to be signed
 * @param datalen the length of data to be signed
 * @param signature a buffer to hold the signature
 * @param siglen the length of the user provided buffer to hold signature
 *
 * @returns siglen the length of the actual signature
 * @returns 0 on success
 * @returns -1 on failure
 */
int xs_sign(const char *xid, unsigned char *data, int datalen, unsigned char *signature, uint16_t *siglen)
{
	char *privkeyhash = NULL;
	int privfilepathlen;
	char *privfilepath = NULL;
	int retval = -1;        // Return failure by default. 0 == success
	int state = 0;

	// Find the directory where keys are stored
	const char *keydir = get_keydir();
	if(keydir == NULL) {
		xs_chatter("xs_sign: Unable to find key directory");
		goto xs_sign_done;
	}
	// Read private key from file in keydir
	privkeyhash = strchr((char *)xid, ':') + 1;
	privfilepathlen = strlen(keydir) + strlen("/") + strlen(privkeyhash) + 1;
	privfilepath = (char *) calloc(privfilepathlen, 1);
	if(privfilepath == NULL) {
		xs_chatter("xs_sign: Unable to allocate memory for priv file path");
		goto xs_sign_done;
	}
	state = 1;
	sprintf(privfilepath, "%s/%s", keydir, privkeyhash);
	if(xs_signWithKey(privfilepath, data, datalen, signature, siglen)) {
		xs_chatter("xs_sign: Failed sign with priv key %s", privfilepath);
		goto xs_sign_done;
	}
	// Success
	retval = 0;

xs_sign_done:
	switch(state) {
		case 1: free(privfilepath);
	};
	return retval;
}

int xs_signDigestWithKey(const char *privfilepath,
		uint8_t *digest, int digest_len,
		unsigned char *signature, uint16_t *siglen)
{
	int rc;
	int retval = -1;
	int state = 0;
	char hex_digest[XIA_SHA_DIGEST_STR_LEN];
	FILE *fp;
	RSA *rsa;
	unsigned char *sig_buf = NULL;
	unsigned int sig_len = 0;

	assert(digest_len >= SHA_DIGEST_LENGTH);

	// Print the SHA1 hash in human readable form
	xs_hexDigest(digest, digest_len, hex_digest, sizeof hex_digest);

    // Encrypt the SHA1 hash with private key
    fp = fopen(privfilepath, "r");
	if(fp == NULL) {
		xs_chatter("xs_sign: ERROR opening private kep file: %s", privfilepath);
		goto xs_sign_digest_with_key_done;
	}
	state = 1;
    rsa = PEM_read_RSAPrivateKey(fp,NULL,NULL,NULL);
    if(rsa==NULL) {
        xs_chatter("xs_sign: ERROR reading private key:%s:", privfilepath);
		goto xs_sign_digest_with_key_done;
	}

	assert(*siglen >= RSA_size(rsa));
    sig_buf = (unsigned char*)calloc(RSA_size(rsa), 1);
	if(!sig_buf) {
		xs_chatter("xs_sign: Failed to allocate memory for signature");
		goto xs_sign_digest_with_key_done;
	}
	state = 2;

    rc = RSA_sign(NID_sha1, digest, digest_len, sig_buf, &sig_len, rsa);
	if(rc != 1) {
		xs_chatter("xs_sign: RSA_sign failed");
		goto xs_sign_digest_with_key_done;
	}

    //xs_chatter("Sig: %X Len: %d", sig_buf[0], sig_len);
    memcpy(signature, sig_buf, sig_len);
	*siglen = (uint16_t) sig_len;

	// Success
	retval = 0;

xs_sign_digest_with_key_done:
	switch(state) {
		case 2:
			free(sig_buf);
			[[gnu::fallthrough]];
		case 1:
			fclose(fp);
			[[gnu::fallthrough]];
	}
	return retval;
}

int xs_signWithKey(const char *privfilepath, unsigned char *data, int datalen,
		unsigned char *signature, uint16_t *siglen)
{
	uint8_t digest[SHA_DIGEST_LENGTH];

	// Calculate SHA1 hash of given data
	xs_getSHA1Hash(data, datalen, digest, sizeof digest);

	return xs_signDigestWithKey(privfilepath, digest, sizeof(digest),
			signature, siglen);
}


/*!
 * @brief Read public key into a buffer from given path
 *
 * The public key file must be present at the provided path.
 *
 * @param pubfilepath location of a file containing the public key
 * @param pubkey a buffer to hold the public key being returned to caller
 * @param pubkey_len the length of public key buffer
 *
 * @returns 0 on success
 * @returns negative value on failure
 */
int xs_readPubkeyFile(const char *pubfilepath,
		char *pubkey, uint16_t *pubkey_len)
{
	FILE *fp;
	int state = 0;
	BIO *bio_pub;
	RSA *rsa;
	int retval = -1;

	// Clear out the user provided buffer in case they forgot to
	bzero(pubkey, *pubkey_len);

	// Read in the public key file
    fp = fopen(pubfilepath, "r");
	if(fp == NULL) {
		xs_chatter("xs_readPubkeyFile: Unable to open %s", pubfilepath);
		retval = -2;
		goto xs_readPubkeyFile_cleanup;
	}
	state = 1;
    rsa = PEM_read_RSA_PUBKEY(fp,NULL,NULL,NULL);
    if(rsa==NULL) {
		xs_chatter("xs_readPubkeyFile: ERROR reading public key:%s:",
				pubfilepath);
		retval = -2;
		goto xs_readPubkeyFile_cleanup;
	}
	state = 2;

    bio_pub = BIO_new(BIO_s_mem());
	if(bio_pub == NULL) {
		xs_chatter("xs_readPubkeyFile: ERROR allocating BIO for public key");
		retval = -3;
		goto xs_readPubkeyFile_cleanup;
	}
	state = 3;
    PEM_write_bio_RSA_PUBKEY(bio_pub, rsa);
	if(*pubkey_len < BIO_pending(bio_pub) + 1) {
		xs_chatter("xs_readPubkeyFile: ERROR pubkey buffer too small");
		retval = -4;
		goto xs_readPubkeyFile_cleanup;
	}
    *pubkey_len = BIO_pending(bio_pub);
    BIO_read(bio_pub, pubkey, *pubkey_len);
	*pubkey_len += 1; // null terminate the string

	// Pubkey has been successfully read into the provided buffer
	retval = 0;

xs_readPubkeyFile_cleanup:
	switch(state) {
		case 3:
			BIO_free_all(bio_pub);
			[[gnu::fallthrough]];
		case 2:
			RSA_free(rsa);
			[[gnu::fallthrough]];
		case 1:
			fclose(fp);
			[[gnu::fallthrough]];
	};
	return retval;
}

/*!
 * @brief Get the public key for a given xid
 *
 * Given an XID, find the public key for it. The public key file must
 * be present on the local disk in the key directory.
 *
 * @param xid the XID whose public key is to be found
 * @param pubkey a buffer to hold the public key
 * @param pubkey_len the length of public key buffer
 *
 * @returns 0 on success
 * @returns negative value on failure
 */
int xs_getPubkey(const char *xid, char *pubkey, uint16_t *pubkey_len)
{
	int pubfilepathlen;
	char *pubfilepath;
	char *pubkeyhash;
	int retval = -1;
	int state = 0;

	// Clear out the user buffer
	memset(pubkey, 0, (size_t)*pubkey_len);

	// Find the directory where keys are stored
	const char *keydir = get_keydir();
	if(keydir == NULL) {
		xs_chatter("xs_getPubkey: ERROR finding key directory");
		retval = -1;
		goto xs_getPubkey_cleanup;
	}
	// Extract the hex portion of the XID
	pubkeyhash = strchr((char *)xid, ':') + 1;
	pubfilepathlen = strlen(keydir) + strlen("/") + strlen(pubkeyhash) + strlen(".pub") + 1;
	pubfilepath = (char *) calloc(pubfilepathlen, 1);
	if(pubfilepath == NULL) {
		xs_chatter("xs_getPubkey: ERROR allocating memory to store public key file path");
		retval = -2;
		goto xs_getPubkey_cleanup;
	}
	state = 1;

	// Build the file path and read the key from the file
	sprintf(pubfilepath, "%s/%s.pub", keydir, pubkeyhash);
	if(xs_readPubkeyFile(pubfilepath, pubkey, pubkey_len)) {
		xs_chatter("xs_getPubkey: ERROR unable to read pubkey from %s",
				pubfilepath);
		retval = -3;
		goto xs_getPubkey_cleanup;
	}
	retval = 0;

xs_getPubkey_cleanup:
	switch(state) {
		case 1: free(pubfilepath);
	};
	return retval;
}

/*!
 * @brief check if hash of the public key matches the XID
 *
 * Generate a SHA-1 hash of the public key and see if it matches XID
 *
 * @param pubkey the public key in string format
 * @param xid the XID in hex hash string
 *
 * @returns 1 if the public key matches the XID
 * @returns 0 if it is not a match
 */
int xs_pubkeyMatchesXID(const char *pubkey, const char *xid)
{
    char pubkeyHash[SHA_DIGEST_LENGTH];
    xs_getPubkeyHash((char *)pubkey, (uint8_t *)pubkeyHash, sizeof pubkeyHash);
    char pubkeyHashStr[XIA_SHA_DIGEST_STR_LEN];
    xs_hexDigest((uint8_t *)pubkeyHash, sizeof pubkeyHash, pubkeyHashStr, sizeof pubkeyHashStr);
    if(strncmp(xs_XIDHash(xid), pubkeyHashStr, sizeof pubkeyHashStr)) {
        xs_chatter("xs_pubkeyMatchesXID: ERROR: Mismatch: pubkeyHash|%s| XID|%s|", pubkeyHashStr, xs_XIDHash(xid));
        return 0;
    }
	return 1;
}

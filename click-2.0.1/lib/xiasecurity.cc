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

/*
// Generate HMAC-SHA1
int xs_HMAC(void* key, int keylen, unsigned char* buf, size_t buflen, unsigned char* hmac, unsigned int* hmac_len)
{
	return 0;
}

// Compare two HMACs
int xs_compareHMACs(unsigned char *hmac1, unsigned char *hmac2, unsigned int hmac_len)
{
	return 0;
}

// Generate a buffer with random data
int xs_makeRandomBuffer(char *buf, int buf_len)
{
	return 0;
}
*/

/*
class XIASecurityBuffer {
    public:
        XIASecurityBuffer(int initialSize);
		XIASecurityBuffer(const char *buf, uint16_t len);
        ~XIASecurityBuffer();
        bool pack(const char *data, uint16_t length);
		bool unpack(char *data, uint16_t *length);
        char *get_buffer();
        uint16_t size();
		uint16_t get_numEntries();
		int peekUnpackLength();
    private:
        bool initialize(); // Called on first pack()
        bool extend(uint32_t length);
		int numEntriesSize;
        int initialSize;
        bool initialized;
        uint16_t *numEntriesPtr;
        char *dataPtr;
        uint32_t remainingSpace;
        char *_buffer;
        char *nextPack;
        char *nextUnpack;
};
*/

void xs_chatter(const char *fmt, ...)
{
	va_list val;
	va_start(val, fmt);
	vfprintf(stderr, fmt, val);
	fprintf(stderr, "\n");
	va_end(val);
}

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

XIASecurityBuffer::XIASecurityBuffer(const char *buf, uint16_t len)
{
	numEntriesSize = sizeof(uint16_t);
	// Copy the serialized buffer
	_buffer = (char *)calloc(len, 1);
	if(_buffer == NULL) {
		xs_chatter("XIASecurityBuffer:: CRITICAL, could not allocate %d bytes for buffer", len);
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

XIASecurityBuffer::~XIASecurityBuffer()
{
	if(_buffer) {
		free(_buffer);
	}
}

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

// Extend buffer to store the length plus length-bytes of data
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

char *XIASecurityBuffer::get_buffer()
{
	if(initialized) {
		return _buffer;
	} else {
		return NULL;
	}
}

uint16_t XIASecurityBuffer::size()
{
	if(initialized) {
		return nextPack - _buffer;
	} else {
		return 0;
	}
}

uint16_t XIASecurityBuffer::get_numEntries()
{
	return *numEntriesPtr;
}

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

// Unpack items from buffer, one per call
// Typical usage: while(unpack(buf, &len)) ... do something
bool XIASecurityBuffer::unpack(char *data, uint16_t *length)
{
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

// Generate SHA1 hash of a given buffer
void xs_getSHA1Hash(const unsigned char *data, int data_len, uint8_t* digest, int digest_len)
{
	assert(digest_len == SHA_DIGEST_LENGTH);
	SHA1(data, data_len, digest);
}

// Convert a SHA1 digest to a hex string
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

// Verify signature using public key read from file for given xid
// NOTE: Public key must be available from local file
int xs_isValidSignature(const unsigned char *data, size_t datalen, unsigned char *signature, unsigned int siglen, const char *xid)
{
	char pem_pub[MAX_PUBKEY_SIZE];
	uint16_t pem_pub_len = MAX_PUBKEY_SIZE;

	// Get the public key for given xid
	if(!xs_getPubkey(xid, pem_pub, &pem_pub_len)) {
		return 0;
	}

	return xs_isValidSignature(data, datalen, signature, siglen, pem_pub, pem_pub_len);
}

// Verify signature using public key in memory
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

const char *xs_XIDHash(const char *xid)
{
	return strchr((char *)xid, ':') + 1;
}

// Sign data using private key of the given xid
int xs_sign(const char *xid, unsigned char *data, int datalen, unsigned char *signature, uint16_t *siglen)
{
	char *privkeyhash = NULL;
	int privfilepathlen;
	char *privfilepath = NULL;
	int retval = -1;        // Return failure by default. 0 == success
	int state = 0;
	unsigned char *sig_buf = NULL;
	unsigned int sig_len = 0;
	int rc;
	RSA *rsa;
	uint8_t digest[SHA_DIGEST_LENGTH];
	char hex_digest[XIA_SHA_DIGEST_STR_LEN];
	FILE *fp;

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

	// Calculate SHA1 hash of given data
	xs_getSHA1Hash(data, datalen, digest, sizeof digest);

	// Print the SHA1 hash in human readable form
	xs_hexDigest(digest, sizeof digest, hex_digest, sizeof hex_digest);
    xs_chatter("xs_sign: Hash of given data: %s", hex_digest);

    // Encrypt the SHA1 hash with private key
    xs_chatter("xs_sign: Signing with private key from: %s", privfilepath);
    fp = fopen(privfilepath, "r");
	if(fp == NULL) {
		xs_chatter("xs_sign: ERROR opening private kep file: %s", privfilepath);
		goto xs_sign_done;
	}
	state = 2;
    rsa = PEM_read_RSAPrivateKey(fp,NULL,NULL,NULL);
    if(rsa==NULL) {
        xs_chatter("xs_sign: ERROR reading private key:%s:", privfilepath);
		goto xs_sign_done;
	}

	assert(*siglen >= RSA_size(rsa));
    sig_buf = (unsigned char*)calloc(RSA_size(rsa), 1);
	if(!sig_buf) {
		xs_chatter("xs_sign: Failed to allocate memory for signature");
		goto xs_sign_done;
	}
	state = 3;

    //int rc = RSA_sign(NID_sha1, digest, sizeof digest, sig_buf, &sig_len, rsa);
    rc = RSA_sign(NID_sha1, digest, sizeof digest, sig_buf, &sig_len, rsa);
	if(rc != 1) {
		xs_chatter("xs_sign: RSA_sign failed");
		goto xs_sign_done;
	}
    xs_chatter("xs_sign: signature length: %d", sig_len);

    //xs_chatter("Sig: %X Len: %d", sig_buf[0], sig_len);
    memcpy(signature, sig_buf, sig_len);
	*siglen = (uint16_t) sig_len;

	// Success
	retval = 0;

xs_sign_done:
	switch(state) {
		case 3: free(sig_buf);
		case 2: fclose(fp);
		case 1: free(privfilepath);
	}
	return retval;
}

// Get the public key for a given xid
int xs_getPubkey(const char *xid, char *pubkey, uint16_t *pubkey_len)
{
	int pubfilepathlen;
	char *pubfilepath;
	char *pubkeyhash;
	int retval = 0;
	int state = 0;
	BIO *bio_pub;
	RSA *rsa;
	FILE *fp;

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
	sprintf(pubfilepath, "%s/%s.pub", keydir, pubkeyhash);
	// Read in the public key file
    fp = fopen(pubfilepath, "r");
	state = 2;
    rsa = PEM_read_RSA_PUBKEY(fp,NULL,NULL,NULL);
    //RSA *rsa = PEM_read_RSAPublicKey(fp,NULL,NULL,NULL);
    if(rsa==NULL) {
		xs_chatter("xs_getPubkey: ERROR reading public key:%s:", pubfilepath);
		retval = -3;
		goto xs_getPubkey_cleanup;
	}
	state = 3;

    bio_pub = BIO_new(BIO_s_mem());
	if(bio_pub == NULL) {
		xs_chatter("xs_getPubkey: ERROR allocating BIO for public key");
		retval = -4;
		goto xs_getPubkey_cleanup;
	}
	state = 4;
    PEM_write_bio_RSA_PUBKEY(bio_pub, rsa);
	if(*pubkey_len < BIO_pending(bio_pub) + 1) {
		xs_chatter("xs_getPubkey: ERROR pubkey buffer too small");
		retval = -5;
		goto xs_getPubkey_cleanup;
	}
    *pubkey_len = BIO_pending(bio_pub);
    BIO_read(bio_pub, pubkey, *pubkey_len);
	*pubkey_len += 1; // null terminate the string
xs_getPubkey_cleanup:
	switch(state) {
		case 4: BIO_free_all(bio_pub);
		case 3: RSA_free(rsa);
		case 2: fclose(fp);
		case 1: free(pubfilepath);
	};
	return retval;
}

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

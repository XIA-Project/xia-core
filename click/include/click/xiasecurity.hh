// -*- c-basic-offset: 4; related-file-name: "../../lib/xiasecurity.cc" -*-
#ifndef XSECURITY_H
#define XSECURITY_H

#include <string>

#define MAX_KEYDIR_PATH_LEN 1024
#define MAX_PUBKEY_SIZE 2048
#define MAX_SIGNATURE_SIZE 256
#define XIA_KEYDIR "key"

#define SHA_DIGEST_LENGTH 20

#define XIA_SHA_DIGEST_STR_LEN SHA_DIGEST_LENGTH*2+1

// Generic buffer to serialize data to be sent over the wire
class XIASecurityBuffer {
    public:
        XIASecurityBuffer(int initialSize);
        XIASecurityBuffer(const char *buf, uint16_t len);
        ~XIASecurityBuffer();
        bool pack(const char *data, uint16_t length);
        bool pack(const char *data);
        bool sign_and_pack(XIASecurityBuffer &payloadbuf,
                const std::string &xid_str);
        bool unpack(char *data, uint16_t *length);
        int peekUnpackLength();
        char *get_buffer();
        uint16_t size();
        uint16_t get_numEntries();
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

/*
// Generate HMAC-SHA1
int xs_HMAC(void* key, int keylen, unsigned char* buf, size_t buflen, unsigned char* hmac, unsigned int* hmac_len);

// Compare two HMACs
int xs_compareHMACs(unsigned char *hmac1, unsigned char *hmac2, unsigned int hmac_len);

// Generate a buffer with random data
int xs_makeRandomBuffer(char *buf, int buf_len);
*/

// Retrieve the hex digest part from a given XID string
const char *xs_XIDHash(const char *xid);

// Generate SHA1 hash of a given buffer
void xs_getSHA1Hash(const unsigned char *data, int data_len, uint8_t* digest, int digest_len);

// Get a hex string representing SHA1 hash of provided data
void xs_getSHA1HexDigest(const unsigned char *data, int data_len,
		char *hex_string, int hex_string_len);

// Get SHA1 hash of a public key string(null terminated)
int xs_getPubkeyHash(char *pubkey, uint8_t *digest, int digest_len);

// Convert a SHA1 digest to a hex string
void xs_hexDigest(uint8_t* digest, int digest_len, char* hex_string, int hex_string_len);


// Verify signature

// XID known
int xs_isValidSignature(const unsigned char *data, size_t datalen,
		unsigned char *signature, unsigned int siglen, const char *xid);
// Pubkey in memory
int xs_isValidSignature(const unsigned char *data, size_t datalen,
		unsigned char *signature, unsigned int siglen,
		char *pem_pub, int pem_pub_len);
// Pubkey file path known
int xs_isValidSignature(const char *pubfilepath,
		const unsigned char *data, size_t datalen,
		unsigned char *signature, unsigned int siglen);
// Pubkey file path known and digest of data provided
int xs_isValidDigestSignature(const char *pubfilepath,
        const unsigned char *digest, size_t digestlen,
        unsigned char *signature, unsigned int siglen);

// Sign a given buffer
int xs_sign(const char *xid, unsigned char *data, int datalen, unsigned char *signature, uint16_t *siglen);

// Sign a given buffer with key stored in given file
int xs_signWithKey(const char *privfilepath, unsigned char *data, int datalen,
		          unsigned char *signature, uint16_t *siglen);

// Sign a given digest with key stored in given file
int xs_signDigestWithKey(const char *privfilepath,
		uint8_t *digest, int digest_len,
		unsigned char *signature, uint16_t *siglen);

// Read public key from provided location
int xs_readPubkeyFile(const char *pubfilepath,
		char *pubkey, uint16_t *pubkey_len);

// Read public key from file related to the provided xid
int xs_getPubkey(const char *xid, char *pubkey, uint16_t *pubkey_len);

// Verify that a given pubkey matches the corresponding XID
int xs_pubkeyMatchesXID(const char *pubkey, const char *xid);
#endif

/*
** Copyright 2013 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
/*!
  @file XSSL_util.c
  @brief Implements SID_from_keypair(),
  					print_keypair(),
					print_bytes(),
					serialize_rsa_pub_key(),
					deserialize_rsa_pub_key(),
					hash(),
					sign(),
					verify(),
					pub_encrypt(),
					priv_decrypt(),
					aes_init(),
					aes_encrypt(),
					aes_decrypt()
*/

#include "xssl.h"
#include "string.h"

/**
* @brief Returns the SHA-1 hash of the supplied public key.
*
* @param keypair
*
* @return Hash as a hex string, prefixed by "SID:".
*/
char* SID_from_keypair(RSA *keypair) {

    //get the modulus and exponent from keypair as hex strings
    char *hexforkey = BN_bn2hex(keypair->n);
    char *hexforexp = BN_bn2hex(keypair->e);
    
    // Hash public key
	SHA_CTX ctx;
	unsigned char *md = (unsigned char*)malloc(SHA_DIGEST_LENGTH);
	if (SHA1_Init(&ctx) == 0) {
		ERROR("ERROR initing SHA1 context");
		return NULL;
	}
	if (SHA1_Update(&ctx, hexforkey, strlen(hexforkey)) == 0) {
		ERROR("ERROR updating SHA1 context with hexforkey");
		return NULL;
	}
	if (SHA1_Update(&ctx, hexforexp, strlen(hexforexp)) == 0) {
		ERROR("ERROR updating SHA1 context with hexforexp");
		return NULL;
	}
	if (SHA1_Final(md, &ctx) == 0) {
		ERROR("ERROR computing digest");
		return NULL;
	}
	
	// format digest as hex string
	char *sid = (char*)malloc(SHA_DIGEST_LENGTH*2 + 4 + 1); // 4 for "SID:"
	sprintf(sid, "SID:");
	for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
		sprintf(&sid[i*2+4], "%02x", md[i]);
	}
    sid[SHA_DIGEST_LENGTH*2+4] = '\0';

	free(hexforkey);
	free(hexforexp);
	free(md);

	return sid;
}


/**
* @brief Prints an RSA keypair.
*
* @param keypair
*/
void print_keypair(RSA *keypair) {
    // To get the C-string PEM form:
    BIO *pri = BIO_new(BIO_s_mem());
    BIO *pub = BIO_new(BIO_s_mem());
    
    PEM_write_bio_RSAPrivateKey(pri, keypair, NULL, NULL, 0, NULL, NULL);
    PEM_write_bio_RSAPublicKey(pub, keypair);
    
    size_t pri_len = BIO_pending(pri);
    size_t pub_len = BIO_pending(pub);
    
	char *pri_key, *pub_key;
    pri_key = (char *)malloc(pri_len + 1);
    pub_key = (char *)malloc(pub_len + 1);
    
    BIO_read(pri, pri_key, pri_len);
    BIO_read(pub, pub_key, pub_len);
    
    pri_key[pri_len] = '\0';
    pub_key[pub_len] = '\0';
    
    printf("\n%s\n\n%s\n", pri_key, pub_key);

    BIO_free_all(pub);
    BIO_free_all(pri);
    free(pri_key);
    free(pub_key);
}

/**
* @brief Print bytes in hex.
*
* @param bytes
* @param len
*/
void print_bytes(unsigned char *bytes, size_t len) {
	
	// format key as hex string
	char str[len*2 + 1];
	for (size_t i = 0; i < len; i++) {
		sprintf(&str[i*2], "%02x", bytes[i]);
	}
    str[len*2] = '\0';
	
	printf("%s\n\n", str);
}


/**
* @brief Serialize the supplied RSA public key to the supplied buffer.
*
* Stores the RSA public key in the supplied buffer. If the space required is
*	larger than len (the size of the buffer), the only the first len bytes will
*	be placed in the buffer. Check that the return value is <= len.
*
* @param keypair The keypair to serialize
* @param buf Buffer for the result.
* @param len Size of the buffer.
*
* @return The size of the serialized keypair. This may be bigger than len,
*	indicating the result has been truncated and should not be used.
*/
uint32_t serialize_rsa_pub_key(RSA *keypair, char *buf, size_t len) {

	size_t bufSpaceLeft = len;

    char *hexformod = BN_bn2hex(keypair->n);
	uint32_t hexformodsize = strlen(hexformod);
    char *hexforexp = BN_bn2hex(keypair->e);
	uint32_t hexforexpsize = strlen(hexforexp);
	DBGF("hexformod: %s (%d)", hexformod, hexformodsize);
	DBGF("hexforexp: %s (%d)", hexforexp, hexforexpsize);

	if (bufSpaceLeft > sizeof(uint32_t)) {
		uint32_t *modsizeptr = (uint32_t*)buf;
		*modsizeptr = hexformodsize;
		bufSpaceLeft -= sizeof(uint32_t);
	}
	if (bufSpaceLeft > hexformodsize) {
		memcpy(&buf[sizeof(uint32_t)], hexformod, hexformodsize);
		bufSpaceLeft -= hexformodsize;
	}
	if (bufSpaceLeft > sizeof(uint32_t)) {
		uint32_t *expsizeptr = (uint32_t*)&buf[sizeof(uint32_t) + hexformodsize];
		*expsizeptr = hexforexpsize;
		bufSpaceLeft -= sizeof(uint32_t);
	}
	if (bufSpaceLeft > hexforexpsize) {
		memcpy(&buf[2*sizeof(uint32_t) + hexformodsize], hexforexp, hexforexpsize);
		bufSpaceLeft -= hexforexpsize;
	}

	uint32_t total_size = strlen(hexformod) + strlen(hexforexp) + 2*sizeof(uint32_t);
	free(hexformod);
	free(hexforexp);

	return total_size;
}

/**
* @brief Deserialize the supplied buffer into an RSA public key.
*
* Allocate a new RSA keypair and fill in the modulus and the exponent from
*	the public key stored in buf.
*
* @param buf Buffer containing the key to deserialize.
* @param len Length of the buffer.
*
* @return RSA *keypair on success, NULL on failure
*/
RSA *deserialize_rsa_pub_key(char *buf, size_t len) {
	(void)len; // TODO: use len to make sure we don't try to read past end of buffer

	/* get modulus */
	uint32_t hexformodsize = *(uint32_t*)buf;
	char *hexformod = (char*)malloc(hexformodsize + 1);
	memcpy(hexformod, &buf[sizeof(uint32_t)], hexformodsize);
	hexformod[hexformodsize] = '\0';
	DBGF("hexformod: %s", hexformod);

	/* get exponent */
	uint32_t hexforexpsize = *(uint32_t*)&buf[sizeof(uint32_t) + hexformodsize];
	char *hexforexp = (char*)malloc(hexforexpsize + 1);
	memcpy(hexforexp, &buf[2*sizeof(uint32_t) + hexformodsize], hexforexpsize);
	hexforexp[hexforexpsize] = '\0';
	DBGF("hexforexp: %s", hexforexp);

	/* make the keypair */
    RSA *keypair = RSA_new();
    BN_hex2bn(&keypair->n, hexformod); // TODO: need BN_new() first?
    BN_hex2bn(&keypair->e, hexforexp);
    keypair->d = NULL;
    keypair->p = NULL;
    keypair->q = NULL;
    keypair->dmp1 = NULL;
    keypair->dmq1 = NULL;
    keypair->iqmp = NULL;

	free(hexformod);
	free(hexforexp);

	return keypair;
}

/**
* @brief Hash the supplied message (SHA-1)
*
* @param msg Message to hash.
* @param msg_len Size of message to hash.
*
* @return Pointer to buffer containing hash on success
* 	NULL on failure
*/
unsigned char *hash(char *msg, size_t msg_len) {
	SHA_CTX ctx;
	unsigned char *digest = (unsigned char*)malloc(SHA_DIGEST_LENGTH);
	if (SHA1_Init(&ctx) == 0) {
		ERROR("ERROR initing SHA1 context");
		return NULL;
	}
	if (SHA1_Update(&ctx, msg, msg_len) == 0) {
		ERROR("ERROR updating SHA1 context");
		return NULL;
	}
	if (SHA1_Final(digest, &ctx) == 0) {
		ERROR("ERROR computing digest");
		return NULL;
	}

	return digest;
}

/**
* @brief Sign a message with provided RSA key.
*
* Hash msg (SHA1) and encrypt the resulting digest. The buffer supplied to hold
* the signature must be at least RSA_size(keypair) bytes long.
*
* @param msg Data to sign.
* @param msg_len Size of data to sign.
* @param sigbuf Buffer to hold created signature.
* @param sigbuflen Space available for signature.
*
* @return Size of signature on success, 0 on error.
*/
uint32_t sign(RSA *keypair, char *msg, size_t msg_len, char *sigbuf, int sigbuflen) {
	
	if (sigbuflen < RSA_size(keypair)) {
		ERROR("ERROR: Could not sign message because sigbuf is too small");
		return 0;
	}

	/* first hash msg */
	unsigned char *digest = hash(msg, msg_len);
	if (digest == NULL) {
		ERROR("ERROR: Unable to hash message");
		return 0;
	}

	/* now sign the hash */
    uint32_t siglen;
    if (RSA_sign(NID_sha1, digest, SHA_DIGEST_LENGTH, (unsigned char*)sigbuf, &siglen, keypair) != 1) {

    	char *err = (char *)malloc(130); //FIXME?
        ERR_load_crypto_strings();
        ERR_error_string(ERR_get_error(), err);
        ERRORF("Error signing message: %s", err);
        
        free(err);
        return 0;
    }

	free(digest);
	return siglen;
}

/**
* @brief Verify a signature with the provided RSA public key.
*
* @param keypair Public key for verifying signature.
* @param msg Message to verify.
* @param msg_len Size of message to verify.
* @param sig Signature to verify.
* @param sig_len Size of signature to verify.
*
* @return 1 on successful verification, 0 otherwise
*/
int verify(RSA *keypair, char *msg, size_t msg_len, char *sig, size_t sig_len) {

	/* first hash msg */
	unsigned char *digest = hash(msg, msg_len);
	if (digest == NULL) {
		ERROR("ERROR: Unable to hash message");
		return 0;
	}

	/* now verify signature */
	int rc = RSA_verify(NID_sha1, digest, SHA_DIGEST_LENGTH, (unsigned char*)sig, sig_len, keypair);

	free(digest);
	return rc;
}

/**
* @brief Encrypt a message using the provided RSA public key.
*
* @param keypair
* @param msg Message to encrypt.
* @param msg_len Size of message to encrypt.
* @param ciphertext_buf Buffer to be filled with ciphertext.
* @param ciphertext_buf_len Size of ciphertext buffer. Should be >= RSA_size(keypair).
*
* @return Size of encrypted message on success, -1 on failure.
*/
int pub_encrypt(RSA *keypair, unsigned char *msg, size_t msg_len, char *ciphertext_buf, int ciphertext_buf_len) {
	if (ciphertext_buf_len < RSA_size(keypair)) {
		ERROR("ERROR: Not enough room for ciphertext");
		return -1;
	}

    int ciphertext_len;
    if((ciphertext_len = RSA_public_encrypt(msg_len, msg, (unsigned char*)ciphertext_buf, keypair, RSA_PKCS1_OAEP_PADDING)) == -1) {
    	char *err = (char *)malloc(130); //FIXME?
        ERR_load_crypto_strings();
        ERR_error_string(ERR_get_error(), err);
        ERRORF("Error encrypting message: %s", err);
        
        free(err);
        return -1;
    }
    
	return ciphertext_len;
}

/**
* @brief Decrpyt a message using the provided RSA private key.
*
* @param keypair
* @param msg_buf Empty buffer for decrypted message.
* @param msg_buf_len Size of message buffer. Should be >= RSA_size(keypair).
* @param ciphertext Ciphertext to decrypt.
* @param ciphertext_len Size of ciphertext.
*
* @return Size of decrypted message on success, -1 on failure.
*/
int priv_decrypt(RSA *keypair, unsigned char *msg_buf, int msg_buf_len, unsigned char *ciphertext, size_t ciphertext_len) {
	if (msg_buf_len < RSA_size(keypair)) {
		ERROR("ERROR: Not enough room for plaintext");
		return -1;
	}

	int plaintext_len;
    if( (plaintext_len = RSA_private_decrypt(ciphertext_len, ciphertext, msg_buf, keypair, RSA_PKCS1_OAEP_PADDING)) == -1) {
    	char *err = (char *)malloc(130); //FIXME?
        ERR_load_crypto_strings();
        ERR_error_string(ERR_get_error(), err);
        ERRORF("error decrypting message: %s", err);
        
        free(err);
		return -1;
    }

	return plaintext_len;
}


/**
 **/
/**
* @brief Prepares AES encryption and decryption contexts.
*
* Create an 256 bit key and IV using the supplied key_data. salt can be added for taste.
* Fills in the encryption and decryption ctx objects and returns 0 on success
*
* Code adapted from: http://saju.net.in/code/misc/openssl_aes.c.txt
*
* @param key_data
* @param key_data_len
* @param salt
* @param e_ctx
* @param d_ctx
*
* @return 0 on success
*/
int aes_init(unsigned char *en_key_data, int en_key_data_len, 
			 unsigned char *de_key_data, int de_key_data_len, 
			 unsigned char *salt, EVP_CIPHER_CTX *e_ctx, 
             EVP_CIPHER_CTX *d_ctx) {
	int i, nrounds = 5;
	unsigned char en_key[32], en_iv[32], de_key[32], de_iv[32];
	
	/*
	 * Gen key & IV for AES 256 CBC mode. A SHA1 digest is used to hash the supplied key material.
	 * nrounds is the number of times the we hash the material. More rounds are more secure but
	 * slower.
	 */
	i = EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha1(), salt, en_key_data, en_key_data_len, nrounds, en_key, en_iv);
	if (i != 32) {
	  ERRORF("Key size is %d bits - should be 256 bits", i);
	  return -1;
	}
	if (EVP_EncryptInit_ex(e_ctx, EVP_aes_256_cbc(), NULL, en_key, en_iv) != 1) {
		ERROR("ERROR initializing encryption context");
		return -1;
	}

	i = EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha1(), salt, de_key_data, de_key_data_len, nrounds, de_key, de_iv);
	if (i != 32) {
	  ERRORF("Key size is %d bits - should be 256 bits", i);
	  return -1;
	}
	if (EVP_DecryptInit_ex(d_ctx, EVP_aes_256_cbc(), NULL, de_key, de_iv) != 1) {
		ERROR("ERROR initializing decryption context");
		return -1;
	}
	
	return 0;
}


/**
* @brief Encrypt *len bytes of data
*
* All data going in & out is considered binary (unsigned char[])
*
* Code adapted from: http://saju.net.in/code/misc/openssl_aes.c.txt
*
* @param e
* @param plaintext
* @param plaintext_len
* @param ciphertext_buf
* @param ciphertext_buf_len
*
* @return Size of ciphertext on success, -1 on failure
*/
int aes_encrypt(EVP_CIPHER_CTX *e, 
				unsigned char *plaintext, int plaintext_len,
				unsigned char *ciphertext_buf, int ciphertext_buf_len) {
	/* max ciphertext len for a n bytes of plaintext is n + AES_BLOCK_SIZE -1 bytes */
	int c_len = plaintext_len + AES_BLOCK_SIZE, f_len = 0;
	if (ciphertext_buf_len < c_len) {
		ERROR("Ciphertext buffer too small");
		return -1;
	}
	
	/* allows reusing of 'e' for multiple encryption cycles */
	if (EVP_EncryptInit_ex(e, NULL, NULL, NULL, NULL) != 1) {
		ERROR("ERROR initializing encryption");
		return -1;
	}
	
	/* update ciphertext, c_len is filled with the length of ciphertext generated,
	  *len is the size of plaintext in bytes */
	if (EVP_EncryptUpdate(e, ciphertext_buf, &c_len, plaintext, plaintext_len) != 1) {
		ERROR("ERROR encrypting");
		return -1;
	}
	
	/* update ciphertext with the final remaining bytes */
	if (EVP_EncryptFinal_ex(e, ciphertext_buf+c_len, &f_len) != 1) {
		ERROR("ERROR encrypting");
		return -1;
	}
	
	return c_len + f_len;
}


/**
* @brief Decrypt *len bytes of ciphertext.
*
* Code adapted from: http://saju.net.in/code/misc/openssl_aes.c.txt
*
* @param e
* @param plaintext_buf
* @param plaintext_buf_len
* @param ciphertext
* @param ciphertext_len
*
* @return Size of decrypted data on success; -1 on failure
*/
int aes_decrypt(EVP_CIPHER_CTX *e, 
				unsigned char *plaintext_buf, int plaintext_buf_len,
				unsigned char *ciphertext, int ciphertext_len) {
	/* because we have padding ON, we must have an extra cipher block size of memory */
	int p_len = ciphertext_len, f_len = 0;
	if (plaintext_buf_len < p_len + AES_BLOCK_SIZE) {
		ERROR("ERROR: plaintext buffer too small");
		return -1;
	}
	
	if (EVP_DecryptInit_ex(e, NULL, NULL, NULL, NULL) != 1) {
		ERROR("ERROR initializing decryption");
		return -1;
	}
	if (EVP_DecryptUpdate(e, plaintext_buf, &p_len, ciphertext, ciphertext_len) != 1) {
		ERROR("ERROR decrypting");
		return-1;
	}
	if (EVP_DecryptFinal_ex(e, plaintext_buf+p_len, &f_len) != 1) {
		ERROR("ERROR decrypting");
		return -1;
	}
	
	return p_len + f_len;;
}

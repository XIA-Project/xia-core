// COMPILE: gcc -o genkeypair genkeypair.c -lcrypto -lssl
/*
** Copyright 2012 Carnegie Mellon University
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
 @file Xkeys.c
 @brief XmakeNewSID(), XremoveSID(), XcreateFID(), XremoveFID() -- create/delete cryptographic SIDs & FIDs
\todo write docs
*/

#include "Xsocket.h"
/*! \cond */
#include <string.h>
#include "Xinit.h"
#include "Xutil.h"
#include "Xkeys.h"

#define KEY_BITS 1024
#define XIA_KEYDIR "key"
#define MAX_KEYDIR_PATH_LEN 1024
#define FID_PREFIX "FID:"
/*! \endcond */


/*!
 * @brief Convert a SHA1 hash to a hex string
 *
 * If a valid SHA-1 digest is provided, this function converts it into a
 * string. If the buffers are not big enough, we assert and terminate
 * the application.
 *
 * @param digest a buffer containing a SHA1 hash
 * @param digest_len length of digest. Must be SHA_DIGEST_LENGTH
 * @param hex_string a buffer to be filled in with SHA1 hash as a string
 * @param hex_string_len length of hex_string buffer
 *
 * @returns void
 */
void sha1_hash_to_hex_string(unsigned char *digest, int digest_len, char *hex_string, int hex_string_len)
{
	int i;
	assert(digest_len == SHA_DIGEST_LENGTH);
	assert(hex_string_len == (2*SHA_DIGEST_LENGTH) + 1);
	for(i=0;i<digest_len;i++) {
		sprintf(&hex_string[2*i], "%02x", (unsigned int)digest[i]);
	}
	hex_string[hex_string_len-1] = '\0';
}

/*!
 * @brief Calculate SHA1 hash of a public key in PEM format
 *
 * \todo in future, we should consider converting the key into binary format
 * and then hashing the binary data instead of the string representation.
 *
 * Strip off the newline characters in the string representation of the
 * provided public key and generate a SHA-1 hash on it.
 *
 * @param hash a buffer to be filled in with SHA-1 hash of pemkey
 * @param hashlen length of hash
 * @param pemkey a buffer containing a public key to be hashed
 * @param keylen length of pemkey buffer
 *
 * @returns 0 on success
 * @return -1 on failure
 */
static int sha1_hash_of_pubkey(unsigned char *hash, int hashlen, char *pemkey, int keylen)
{
	int i, j;
	char *pubkeystr;
	int keystartoffset = strlen("-----BEGIN PUBLIC KEY-----\n");
	int keyendoffset = strlen("-----END PUBLIC KEY-----\n");

	assert(hash != NULL);
	assert(hashlen == SHA_DIGEST_LENGTH);

	pubkeystr = (char *)calloc(keylen - keystartoffset - keyendoffset, 1);
	if(pubkeystr == NULL) {
		return -1;
	}
	for(i=keystartoffset,j=0; i<keylen-keyendoffset-1; i++) {
		if(pemkey[i] != '\n') {
			pubkeystr[j++] = pemkey[i];
		}
	}
	//printf("Stripped Pubkey:%s:\n", pubkeystr);
	SHA1((const unsigned char *)pubkeystr, strlen(pubkeystr), hash);
	free(pubkeystr);
	return 0;
}

/*!
 * @brief Write keys in PEM format to filesystem
 *
 * For a given RSA key-pair, and hash of the public key, two files will be
 * created in the key directory: <hash> and <hash>.pub, where the files
 * contain the private and the public keys respectively.
 *
 * @praam keydir a string containing the key directory location on filesystem
 * @param pubkeyhashstr a string containing hash of the public key
 * @param r RSA key pair to be written to disk
 *
 * @returns 0 on success
 * @returns -1 on failure
 */
static int write_key_files(const char *keydir, char *pubkeyhashstr, RSA *r)
{
	int state = 0;
	int retval = -1;
	BIO *pubfilebio = NULL;
	BIO *privfilebio = NULL;
	char *pubfilepath = NULL;
	char *privfilepath = NULL;
	int filepathlen = strlen(keydir) + strlen("/") + strlen(pubkeyhashstr) + 1;

	privfilepath = (char *)calloc(filepathlen, 1);
	if(privfilepath == NULL) {
		LOG("ERROR: Allocating memory for private file name");
		goto cleanup_write_key_files;
	}
	state = 1;

	pubfilepath = (char *)calloc(filepathlen + strlen(".pub"), 1);
	if(pubfilepath == NULL) {
		LOG("ERROR: Allocating memory for public file name");
		goto cleanup_write_key_files;
	}
	state = 2;

	sprintf(privfilepath, "%s/%s", keydir, pubkeyhashstr);
	sprintf(pubfilepath, "%s.pub", privfilepath);
	pubfilebio = BIO_new_file(pubfilepath, "w+");
	if(pubfilebio == NULL) {
		LOGF("ERROR: creating %s in %s", pubfilepath, keydir);
		goto cleanup_write_key_files;
	}
	state = 3;

	if(PEM_write_bio_RSA_PUBKEY(pubfilebio, r) != 1) {
		LOGF("ERROR: writing %s in %s", pubfilepath, keydir);
		goto cleanup_write_key_files;
	}
	privfilebio = BIO_new_file(privfilepath, "w+");
	if(privfilebio == NULL) {
		LOGF("ERROR: creating %s in %s", privfilepath, keydir);
		goto cleanup_write_key_files;
	}
	state = 4;

	if(PEM_write_bio_RSAPrivateKey(privfilebio, r, NULL, NULL, 0, NULL, NULL) != 1) {
		LOGF("ERROR: writing to %s in %s", privfilepath, keydir);
		goto cleanup_write_key_files;
	}

	retval = 0;

cleanup_write_key_files:
	switch(state) {
		case 4: BIO_free_all(privfilebio);
		case 3: BIO_free_all(pubfilebio);
		case 2: free(pubfilepath);
		case 1: free(privfilepath);
	}
	return retval;
}

/*!
 * @brief check if the file exists and is a regular file
 *
 * stat the requested file and ensure it is a regular file.
 *
 * @param filepath location of file on the filesystem
 *
 * @returns 1 if the file exists
 * @returns 0 if the file does not exist
 */
static int file_exists(char *filepath)
{
	struct stat fileinfo;
	if(stat(filepath, &fileinfo)) {
		return 0;
	}
	if(!S_ISREG(fileinfo.st_mode)) {
		return 0;
	}
	return 1;
}

/*!
 * @brief checkif the requested directory exists
 *
 * stat the requested directory path and ensure that it is a directory.
 *
 * @param keydir the directory to be checked for existence
 *
 * @returns 1 if the directory exists
 * @returns 0 if the directory is not found
 */
static int dir_exists(const char *keydir)
{
	struct stat keydirinfo;

	// Verify that keydir exists
	if(stat(keydir, &keydirinfo)) {
		return 0;
	}
	//if(!keydirinfo.st_mode & S_IFDIR) {
	if(!S_ISDIR(keydirinfo.st_mode)) {
		return 0;
	}
	return 1;
}

/*!
 * @brief Find the location of key directory
 *
 * Once found we never look for it again
 * NOTE: Call must NOT free the pointer returned
 * memory is allocated for the key directory only once.
 *
 * @returns NULL if the key directory is not found
 * @returns null-terminated string containing key directory, if found
 */
static const char *get_keydir()
{
	static const char *keydir = NULL;
	if(keydir == NULL) {
		keydir = (const char *)calloc(MAX_KEYDIR_PATH_LEN, 1);
		if(keydir == NULL) {
			LOG("ERROR: Allocating memory to store key dir name");
			return NULL;
		}
		if(XrootDir((char *)keydir, MAX_KEYDIR_PATH_LEN) == NULL) {
			LOG("ERROR: Unable to get XIA root directory name");
			return NULL;
		}
		strcat((char *)keydir, "/");
		strcat((char *)keydir, XIA_KEYDIR);
	}
	return keydir;
}


/*!
 * @brief delete the key pair matching the public key hash provided by caller
 *
 * We keep key files in the key directory with filenames matching the hash
 * of the public key in the key pair. This function allows the caller to
 * delete files associated with the requested key pair.
 *
 * @param pubkeyhashstr hash string of pubkey, for pair to be deleted
 * @param hashstrlen length of provided hash of pubkey
 *
 * @returns 0 on success
 * @returns -1 on failure
 */
int destroy_keypair(const char *pubkeyhashstr, int hashstrlen)
{
	char *privfilepath;
	char *pubfilepath;
	int privfilepathlen;
	int pubfilepathlen;
	int rc = 0;
	int state = 0;
	const char *keydir = get_keydir();
	if(keydir == NULL) {
		rc = -1;
		LOG("destroy_keypair: ERROR: Key directory not found");
		goto destroy_keypair_done;
	}

	privfilepathlen = strlen(keydir) + strlen("/") + hashstrlen + 1;
	pubfilepathlen = privfilepathlen + strlen(".pub");

	privfilepath = (char *)calloc(privfilepathlen, 1);
	if(privfilepath == NULL) {
		rc = -1;
		LOG("destroy_keypair: ERROR: Out of memory");
		goto destroy_keypair_done;
	}
	state = 1;
	pubfilepath = (char *)calloc(pubfilepathlen, 1);
	if(pubfilepath == NULL) {
		rc = -1;
		LOG("destroy_keypair: ERROR: Memory not available");
		goto destroy_keypair_done;
	}
	state = 2;
	strcat(privfilepath, keydir);
	strcat(privfilepath, "/");
	strncat(privfilepath, pubkeyhashstr, hashstrlen);
	strcat(pubfilepath, privfilepath);
	strcat(pubfilepath, ".pub");
	if(unlink(privfilepath)) {
		rc = -1;
		LOGF("destroy_keypair: ERROR: removing %s", privfilepath);
		goto destroy_keypair_done;
	}
	if(unlink(pubfilepath)) {
		rc = -1;
		LOGF("destroy_keypair: ERROR: removing %s", pubfilepath);
		goto destroy_keypair_done;
	}
destroy_keypair_done:
	switch(state) {
		case 2:
			free(pubfilepath);
		case 1:
			free(privfilepath);
	};
	return rc;
}

/*!
 * @brief remove keys associated with given SID
 *
 * An SID is a hash of the public key preceded by the string "SID:".
 * We remove the key files associated with the given SID.
 *
 * @param sid string representing the SID
 *
 * @returns 0 on successful deletion of key-pair files associated with sid
 * @returns -1 on failure
 */
int XremoveSID(const char *sid)
{
	int sidhashlen = strlen(sid) - 4;
	char sid_hash[sidhashlen+1];
	strcpy(sid_hash, &sid[4]);
	return destroy_keypair(sid_hash, sidhashlen);
}

/*!
 * @brief generate a cryptographic key-pair
 *
 * Create a new cryptographic key pair. The keys are stored in the key
 * directory and a hash of the public key is provided to the caller as
 * a handle to reference the key pair in future.
 *
 * @param pubkeyhashstr buffer to return hash of newly generated public key
 * @param hashstrlen length of pubkeyhashstr buffer
 *
 * @returns 0 on success
 * @returns -1 on failure
 */
int generate_keypair(char *pubkeyhashstr, int hashstrlen)
{
	int retval = -1;
	int state = 0;
	int keylen;
	RSA *r = NULL;
	BIGNUM *bne = NULL;
	BIO *pubkeybuf = NULL;
	char *pubkeystr = NULL;
	unsigned char pubkeyhash[SHA_DIGEST_LENGTH];
	unsigned long e = RSA_F4;

	// Get location of key directory
	const char *keydir = get_keydir();
	if(keydir == NULL) {
		LOG("generate_keypair: ERROR: Key directory not found");
		goto cleanup_generate_keypair;
	}

	// Check that the key directory exists
	if(!dir_exists(keydir)) {
		LOG("generate_keypair: ERROR: No key directory");
		goto cleanup_generate_keypair;
	}

	// Check if we can write to the key directory
	if(access(keydir, W_OK)) {
		LOGF("generate_keypair: ERROR: Cannot write to %s", keydir);
		LOGF("generate_keypair: Check permissions - chmod 777 %s", keydir);
		goto cleanup_generate_keypair;
	}

	// Create BIGNUM argument for key generation
	bne = BN_new();
	if(bne == NULL) {
		LOG("generate_keypair: ERROR creating BIGNUM object");
		goto cleanup_generate_keypair;
	}
	state = 1;

	if(BN_set_word(bne, e) != 1) {
		LOG("generate_keypair: ERROR setting BIGNUM to RSA_F4");
		goto cleanup_generate_keypair;
	}

	// Create a new RSA key pair
	r = RSA_new();
	if(RSA_generate_key_ex(r, KEY_BITS, bne, NULL) != 1) {
		LOG("generate_keypair: ERROR: RSA_generate_key_ex");
		goto cleanup_generate_keypair;
	}
	state = 2;

	// Convert public key into a string

	pubkeybuf = BIO_new(BIO_s_mem());
	if(pubkeybuf == NULL) {
		LOG("generate_keypair: ERROR: Creating buffer for public key");
		goto cleanup_generate_keypair;
	}
	state = 3;

	if(PEM_write_bio_RSA_PUBKEY(pubkeybuf, r) != 1) {
		LOG("generate_keypair: ERROR: Writing public key to buffer");
		goto cleanup_generate_keypair;
	}

	keylen = BIO_pending(pubkeybuf);
	pubkeystr = (char *)calloc(keylen+1, 1);
	if(pubkeystr == NULL) {
		LOG("generate_keypair: ERROR: allocating memory for pubkey str");
		goto cleanup_generate_keypair;
	}
	state = 4;

	if(BIO_read(pubkeybuf, pubkeystr, keylen) <= 0) {
		LOG("generate_keypair: ERROR: writing pubkey from buffer to str");
		goto cleanup_generate_keypair;
	}

	// Get SHA1 hash of pubkey string - XID for this key pair

	if(sha1_hash_of_pubkey(pubkeyhash, SHA_DIGEST_LENGTH, pubkeystr, keylen+1)) {
		LOG("generate_keypair: ERROR: getting sha1 hash of pubkey");
		goto cleanup_generate_keypair;
	}

	sha1_hash_to_hex_string(pubkeyhash, SHA_DIGEST_LENGTH, pubkeyhashstr, hashstrlen);

	// Store keys in filename based on XID created above
	if(write_key_files(keydir, pubkeyhashstr, r)) {
		LOGF("generate_keypair: ERROR writing %s to %s", pubkeyhashstr, keydir);
		goto cleanup_generate_keypair;
	}

	// Successfully written files
	retval = 0;

cleanup_generate_keypair:
	switch(state) {
		case 4: free(pubkeystr);
		case 3: BIO_free_all(pubkeybuf);
		case 2: RSA_free(r);
		case 1: BN_free(bne);
	};

	return retval;
}

/*!
 * @brief make a new SID backed by a crypto key-pair
 *
 * Create a new session identifier that an application can use to uniquely
 * identify itself in XIA.
 *
 * @param randomSID a buffer to return the newly generated SID
 * @param randomSIDlen length of randomSID buffer
 *
 * @returns 0 on success
 * @return -1 on failure
 */
int XmakeNewSID(char *randomSID, int randomSIDlen)
{
	char pubkeyhashstr[XIA_SHA_DIGEST_STR_LEN];
	assert(randomSIDlen >= (int) (strlen("SID:") + XIA_SHA_DIGEST_STR_LEN));
	if(generate_keypair(pubkeyhashstr, XIA_SHA_DIGEST_STR_LEN)) {
		return -1;
	}
	strcpy(randomSID, "SID:");
	strcat(randomSID, pubkeyhashstr);
	return 0;
}

int XmanageFID(const char *fid, bool create)
{
	int rc = -1;
	int sock = MakeApiSocket(SOCK_DGRAM);
	xia::XSocketMsg xsm;
	xia::X_ManageFID_Msg *xfm;

	if (sock <= 0) {
		LOG("Unable to create socket");
		return rc;
	}

	xfm = xsm.mutable_x_manage_fid();
	xsm.set_type(xia::XMANAGEFID);
	xsm.set_sequence(0);
	xsm.set_id(0);
	xfm->set_create(create);
	xfm->set_fid(fid);

	if ((rc = click_send(sock, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));

	} else if ((rc = click_reply(sock, 0, &xsm)) < 0) {
		LOGF("Error getting status from Click: %s", strerror(errno));
	}

	freeSocketState(sock);
	(_f_close)(sock);
	return rc;
}

/*!
** @brief create and register an FID
**
** Returns a text string containing a FID made from the a 160 bit cryptographic hash
** from a newly created public/private keypair in the form of <code>FID:nnnnn....</code>.
**
** The new FID is also registered in the local routing table so that destination
** DAGs using the FID will be handled locally.
**
** @param fid  a buffer to receive the newly created FID.
** @param len  the length of fid. If less then 45 characters an error will be returned.
**
** @returns 0 on success
** @returns -1 on error with errno set
*/
int XcreateFID(char *fid, int len)
{
	int tlen = strlen(FID_PREFIX);
	assert(len >= (int)(tlen + XIA_SHA_DIGEST_STR_LEN));
	char *p = fid + tlen;

	memset(fid, 0, len);
	strcpy(fid, FID_PREFIX);
	if (generate_keypair(p, XIA_SHA_DIGEST_STR_LEN)) {
		return -1;
	}

	// now tell click to bind to it
	return XmanageFID(fid, true);
}

/*!
** @brief delete and un-register an FID
**
** Remove the FID from the local routing table and delete
** the associated keypair.
**
** @param fid  the FID to remove
**
** @returns 0 on success
** @returns -1 on error
*/
int XremoveFID(const char *fid)
{
	int tlen = strlen(FID_PREFIX);
	const char *p = fid + tlen;
	int len = strlen(p);
	assert(len >= XIA_SHA_DIGEST_STR_LEN - 1);

	if (strncasecmp(fid, FID_PREFIX, tlen) != 0) {
		LOGF("%s is not a valid FID", fid);
		return -1;
	}

	if (XmanageFID(fid, false) < 0) {
		return -1;
	}

	return destroy_keypair(p, len);
}

/*!
 * @brief Check that key files matching pubkeyhashstr exist in keydir
 *
 * Given a public-key hash, check if the corresponding key-pair is
 * on the local disk, available for signing or encrypting.
 *
 * @param pubkeyhashstr Hash of a public key
 *
 * @returns 1 if the keys are available.
 * @returns 0 if keys are not found.
 */
int exists_keypair(const char *pubkeyhashstr)
{
	int retval = 0;
	int state = 0;
	char *privfilepath;
	char *pubfilepath;
	int privfilepathlen;
	int pubfilepathlen;
	const char *keydir = get_keydir();
	if(keydir == NULL) {
		LOG("exists_keypair: ERROR: Key directory not found");
		goto exists_keypair_done;
	}

//	LOGF("Key directory:%s:", keydir);
	if(!dir_exists(keydir)) {
		LOG("Key directory does not exist");
		goto exists_keypair_done;
	}
	privfilepathlen = strlen(keydir) + strlen("/") + strlen(pubkeyhashstr) + 1;
	pubfilepathlen = privfilepathlen + strlen(".pub");
	privfilepath = (char *)calloc(privfilepathlen, 1);
	if(privfilepath == NULL) {
		LOG("ERROR: Allocating memory to store private-file path");
		goto exists_keypair_done;
	}
	state = 1;
	pubfilepath = (char *)calloc(pubfilepathlen, 1);
	if(pubfilepath == NULL) {
		LOG("ERROR: Allocating memory to store public-file path");
		goto exists_keypair_done;
	}
	state = 2;
	sprintf(privfilepath, "%s/%s", keydir, pubkeyhashstr);
	sprintf(pubfilepath, "%s.pub", privfilepath);
	if(file_exists(privfilepath) && file_exists(pubfilepath)) {
		retval = 1;
	}
exists_keypair_done:
	switch(state) {
		case 2: free(pubfilepath);
		case 1: free(privfilepath);
	}
	return retval;
}

/*!
 * @brief check if keys corresponding to an SID exist locally
 *
 * @param sid Session Identifier whose keys we are looking for
 *
 * @returns 1 if keys corresponding to SID are available.
 * @returns 0 if the keys for the given SID are not found.
 */
int XexistsSID(const char *sid)
{
	return exists_keypair(&sid[4]);
}

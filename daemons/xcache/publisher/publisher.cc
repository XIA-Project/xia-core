#include <assert.h>
#include <string.h>
#include <Xsocket.h>
#include <Xsecurity.h>
#include "publisher.h"

/*!
 * @brief An Object representing a Publisher named on instantiation
 */
Publisher::Publisher(std::string name)
{
	char keydir[MAX_KEYDIR_PATH_LEN];
	std::string publisher_dir = "publisher/" + name;
	if(XrootDir((char *)keydir, MAX_KEYDIR_PATH_LEN) == NULL) {
		printf("ERROR: Unable to get XIA root directory name");
		assert(-1);
	}

	// Protect from buffer overflow attacks on strcat
	assert(strlen(keydir) + publisher_dir.size() < MAX_KEYDIR_PATH_LEN);
	_keydir = strcat(keydir, publisher_dir.c_str());

	_name = name;
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
 */
std::string Publisher::pubkey()
{
	char pubkeybuf[MAX_PUBKEY_SIZE];
	uint16_t pubkeylen = MAX_PUBKEY_SIZE;

	// Pubkey should be in a file in the credential directory
	std::string pubkeypath = keydir() + "/" + name() + ".pub";
	// Find the pubkey file in the Publisher's credential directory
	if(xs_readPubkeyFile(pubkeypath.c_str(), pubkeybuf, &pubkeylen)) {
		printf("Publisher::pubkey() cannot read key from %s\n",
				pubkeypath.c_str());
	}
	std::string pubkeystr(pubkeybuf, pubkeylen);
	return pubkeystr;
}

/*!
 * @brief calculate NCID for the given content name
 *
 * NOTE: Returned string does not have "NCID:" prepended to it
 * @returns Hex string representing the NCID for given content name
 */
std::string Publisher::ncid(std::string content_name)
{
	std::string URI = content_URI(content_name);
	std::string pubkeystr = pubkey();
	std::string ncid_data = URI + pubkeystr;
	char ncidhex[XIA_SHA_DIGEST_STR_LEN];
	int ncidlen = XIA_SHA_DIGEST_STR_LEN;
	xs_getSHA1HexDigest((const unsigned char *)ncid_data.c_str(),
			ncid_data.size(), ncidhex, ncidlen);
	std::string ncidstr(ncidhex, ncidlen);
	return ncidstr;

}

/*!
 * @brief Directory containing all Publisher security credentials
 */
std::string Publisher::keydir()
{
	return _keydir;
}

std::string Publisher::privfilepath()
{
	std::string privkeypath = keydir() + "/" + name() + ".key";
	return privkeypath;
}

/*!
 * @brief Sign the given content to associate it with given content_URI
 */
int  Publisher::sign(std::string content_URI,
		const std::string &content,
		std::string &signature)
{
	int retval = -1;
	unsigned char sig_buf[MAX_SIGNATURE_SIZE];
	uint16_t siglen = MAX_SIGNATURE_SIZE;

	// Sign (Content URI + Content)
	std::string data = content_URI + content;
	// Private file path
	std::string privkeyfile = privfilepath();

	if(xs_signWithKey(privkeyfile.c_str(),
				(unsigned char *)data.c_str(), (int) data.size(),
				sig_buf, &siglen)) {
		printf("Publisher::sign() failed signing provided content\n");
		return -1;
	}
	std::string sigstr((char *)sig_buf, siglen);
	signature = sigstr;
	return retval;
}


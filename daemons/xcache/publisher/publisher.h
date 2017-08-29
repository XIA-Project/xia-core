#ifndef _PUBLISHER_H
#define _PUBLISHER_H
#include <assert.h>
#include <string.h>
#include <Xsocket.h>
#include <Xsecurity.h>

/*!
 * @brief Represent a Publisher and its local capabilities
 *
 * In presence of Publisher private key, this object is capable of
 * signing named content.
 *
 * On a client downloading named content, this object is capable of
 * verifying the said content if the Publisher's certificate is
 * available
 */
class Publisher {
	public:
		Publisher(std::string name);
		~Publisher();
		std::string name();
		std::string content_URI(std::string content_name);
		std::string ncid(std::string content_name);
		int sign(std::string content_URI, const std::string &content,
				std::string &signature);
	private:
		std::string privfilepath();
		std::string pubkey();
		std::string keydir();

		bool pubkey_present(std::string path);
		bool fetch_pubkey();
		std::string cert_name();

		bool keydir_present();
		bool ensure_keydir_exists();
		bool store_publisher_cert(void *cert, size_t len,std::string &certpath);

		const char *_keydir;
		std::string _name;
};
#endif // _PUBLISHER_H

#ifndef _PUBLISHER_KEY_H
#define _PUBLISHER_KEY_H
#include <assert.h>
#include <string.h>
#include <Xsocket.h>
#include <xcache.h>
#include <Xsecurity.h>
#include <dagaddr.hpp>

// C++ header
#include <memory>

/*!
 * @brief Represent a PublisherKey and its local capabilities
 *
 * In presence of PublisherKey private key, this object is capable of
 * signing named content.
 *
 * On a client downloading named content, this object is capable of
 * verifying the said content if the PublisherKey's certificate is
 * available
 */
class PublisherKey {
	public:
		PublisherKey(std::string name);
		~PublisherKey();
		std::string name();
		std::string pubkey();
		std::string cert_dag_str();
		int sign(std::string &digest, std::string &signature);
		int checkSignature(std::string &digest, std::string &signature);
	private:
		bool pubkey_present(std::string path);
		bool fetch_pubkey();

		std::string privfilepath();
		std::string pubfilepath();
		std::string keydir();

		std::string cert_name();
		bool fetch_cert_dag();

		bool keydir_present();
		bool ensure_keydir_exists();
		bool store_publisher_cert(void *cert, size_t len,std::string &certpath);

		const char *_keydir;
		std::string _name;
		std::unique_ptr<Graph> _cert_dag;
		XcacheHandle _h;
};

#endif // _PUBLISHER_KEY_H

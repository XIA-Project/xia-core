#ifndef _PUBLISHER_H
#define _PUBLISHER_H
#include <assert.h>
#include <string.h>
#include <Xsocket.h>
#include <Xsecurity.h>
#include <dagaddr.hpp>

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
		std::string ncid_dag(std::string content_name);
		int sign(std::string content_URI, const std::string &content,
				std::string &signature);
		bool isValidSignature(std::string content_URI,
				const std::string &content,
				const std::string &signature);
	private:
		std::string pubkey();
		std::string content_dag();

		std::string _name;
		Graph *_cert_dag;
		std::string _pubkey;
};
#endif // _PUBLISHER_H

#include <assert.h>
#include <string.h>
#include <Xsocket.h>
#include <Xsecurity.h>

class Publisher {
	public:
		Publisher(std::string name);
		//~Publisher();
		std::string name();
		std::string content_URI(std::string content_name);
		std::string ncid(std::string content_name);
		int sign(std::string content_URI, std::string &content,
				std::string &signature);
	private:
		std::string privfilepath();
		std::string pubkey();
		std::string keydir();

		const char *_keydir;
		std::string _name;
};

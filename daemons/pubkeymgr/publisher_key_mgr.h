// Project headers
#include "publisher_key_mgmt.pb.h"

// System headers
#include <utility> // pair
#include <future>
#include <string>
#include <vector>

class PublisherKeyMgr {
	using ResultFuture = std::shared_future<int>;
	using ResultFutures = std::vector<ResultFuture>;

	ResultFutures results;
	int _sockfd;		// Management socket for clients to connect to

	int process(int sockfd);
	void send_response(int fd, PublisherKeyResponseBuf &resp);
	void handle_key_request(int fd, const PublisherKeyRequest &req);
	void handle_sign_request(int fd, const PublisherSignRequest &req);
	void handle_verify_request(int fd, const PublisherVerifyRequest &req);

public:
	void manage();
	PublisherKeyMgr();
	~PublisherKeyMgr();
};

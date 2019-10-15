#define CONFFILE "local.conf"
#define THEIR_ADDR "THEIR_ADDR" // The THEIR_ADDR entry in config file
#define CLIENT_AID "CLIENT_AID" // The CLIENT_AID entry in config file
#define SERVER_AID "SERVER_AID" // The SERVER_AID entry in config file
#define TICKET_STORE "TICKET_STORE"


// create port


void getmy_dag(GraphPtr mydag) {
	
	// bind on config port

	// set up tcp connection and send ping to configurator

	// listen for config string

	// fill mydag

	// connect to the xia router from the dag

	// spin up a thread to listen for future update

}

void update_mydag(GraphPtr mydag, socket config_socket) {
	
	// listen

	// data 

	// bind socket

	// make connection

	// update mydag

	// clean up the old connection
}

auto conf = LocalConfig::get_instance(CONFFILE);
	auto server_addr = conf.get(THEIR_ADDR);
	auto server_aid = conf.get(SERVER_AID);
	auto client_aid = conf.get(CLIENT_AID);
	const auto ticket_store_filename = conf.get(TICKET_STORE);
	GraphPtr mydag;
	sockaddr_x my_address;
	int my_addrlen;


// A socket to talk to server on
	//sockfd = socket(server_address.ss_family, SOCK_DGRAM, IPPROTO_UDP);
	sockfd = picoquic_xia_open_server_socket(client_aid.c_str(), mydag);
	if(sockfd == INVALID_SOCKET) {
		goto client_done;
	}
	std::cout << "CLIENTADDR: " << mydag->dag_string() << std::endl;
	mydag->fill_sockaddr(&my_address);
	my_addrlen = sizeof(sockaddr_x);
	printf("Created socket to talk to server\n");
	state = 1; // socket created
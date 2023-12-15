#include "xiaforwardingtable.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <ifaddrs.h>
#define ROUTER_PORT 8770
#define testifaddr "172.65.4.1"
#define MSGBUFSIZE 1500
#include "dagaddr.hpp"

using namespace std;

XIAforwardingtable::XIAforwardingtable():route_table() {
        this->_sockfd = -1;
	this->_hastable= 0;
}

/**
 * Overlay socket creation on router
 **/

int XIAforwardingtable::init_socket (string ip, string rname){
	struct addrinfo hints,*server;
        struct sockaddr client;
        socklen_t client_size;
        int r,sockfd;
        //const int size = 1024;
        const size_t maxBytes = MSGBUFSIZE - 1;
        char c_msg[MSGBUFSIZE],output[MSGBUFSIZE];

        //step1. server configuration
        memset(&hints, 0, sizeof(hints));       /* use memset_s() */
        hints.ai_family = AF_INET;                      /* IPv4 connection */
        hints.ai_socktype = SOCK_DGRAM;         /* UDP, datagram */
        hints.ai_flags = AI_PASSIVE;            /* accept any connection */
        r = getaddrinfo("172.65.1.1", "8770", &hints, &server);    /* 0==localhost */
        if( r!=0 )
        {
                perror("failed");
                exit(1);
        }

        //step2. open socket with domain,type and protocol
        sockfd = socket(server->ai_family,server->ai_socktype,server->ai_protocol);
        printf("test open listening socket at router!!!!%d\n", sockfd);
        if( sockfd==-1 )
        {
                perror("failed");
                exit(1);
        }
	//step3. bind server to socket
        r = bind( sockfd,server->ai_addr,server->ai_addrlen);
        if( r==-1 )
        {
                perror("failed");
                exit(1);
        }
        printf("UDP server is listening for client to send something......\n");

        //step4. obtain client input from recv function; &client is referred as clientAddr used for later nameinfo call
        client_size = sizeof(struct sockaddr);
        r = recvfrom(sockfd,c_msg, maxBytes,0,&client,&client_size);
        if( r==-1 )
        {
                perror("failed");
                exit(1);
        }
        //step5. process data to get the client name and sent response
        //NI_NUMERICHOST: as flag return IPAddress as human readable format string
        getnameinfo(&client,client_size,output,maxBytes,0,0,NI_NUMERICHOST);
        printf("receiving client IP %s size %d\n", output, r);
	// send back their IP address
        int retval=sendto(sockfd,output,strlen(output),0,&client,client_size);

        //step6. clean-up and close
        freeaddrinfo(server);
        close(sockfd);

        return retval;
}


int XIAforwardingtable::get_socketfd()
{
        return this->_sockfd;
}

int XIAforwardingtable::get_tableflg()
{
        return this->_hastable;
}

void XIAforwardingtable::add_to_table(unordered_map<string, RouterEntry>& router_table, RouterEntry entry) {
    router_table[entry.xid] = entry;
}

void XIAforwardingtable::delete_from_table(unordered_map<string, RouterEntry>& router_table, string xid) {
    router_table.erase(xid);
}

//moniter incoming packet, then send to from specific port
void XIAforwardingtable::incomingPacket() {
	std::cout<<"HERE WE GO"<<std::endl;
}
/**
 * Print the current routes hashtable*
 * @param routerEntry hashtable
 * @return void
 **/
void XIAforwardingtable::display_table(const unordered_map<string, RouterEntry> &router_table) {

  cout << left << setw(8) <<"TYPE"<<left<<setw(45)<<"XID"<<left<<setw(8)<<"PORT"<<left<<setw(13)
            <<" NEXT HOP" <<left<<setw(8)<<"FLAG"<<endl;

  for(const auto& keyVal : router_table) {
    cout << left <<setw(8)<<(keyVal.second).type <<left<<setw(45) << keyVal.first
             << left <<setw(8) << (keyVal.second).port <<left<< setw(20)<<(keyVal.second).nextHop
             << right<<setw(8)<< hex <<setfill('0')<< atoi(((keyVal.second).flags).c_str())<<setfill(' ') << endl; 
  }
}

 /**
 * Retrieve the route entry in routerEntry hashtable
 * @param routerEntry hashtable 
 * @param  - XID
 * @return  - iterator of the located routeEntry, NULL if not found in hashtable
 **/

 int XIAforwardingtable::lookup_Route(unordered_map<string, RouterEntry>& _rt, string xid ) {
	unordered_map<string, RouterEntry>::iterator itr = _rt.find(xid);
	int retVal =0;
	if (itr != _rt.end()) {
		cout<<"\nRouteEntry is existing already! " <<itr->first<<endl;
	} else {
		cout<<"\nRouteEntry is not found. Adding to routetable ..."<<endl;
		retVal= -1;
	}
	return retVal;
}

void XIAforwardingtable::write_table_to_file(string fpath, unordered_map<string, RouterEntry>& router_table) {
    string fname = fpath + "xiaforwardingdata.csv";
    ofstream file(fname);
    if (file.is_open()) {
        for (unordered_map<string, RouterEntry>::iterator it = router_table.begin(); it != router_table.end(); ++it) {
            file << it->second.type << "," << it->second.xid << "," << it->second.port << "," 
		    <<it->second.nextHop<<","<< it->second.flags << "\n";
        }
        file.close();
    }
    else {
        cerr << "Error: Could not open file " << fname.c_str()<< " for writing" << endl;
    }
}

void XIAforwardingtable::read_table_from_file(string fpath, unordered_map<string, RouterEntry>& router_table) {
    string fname = fpath + "xiaforwardingdata.csv";
    FILE *cf;
    ifstream file(fname.c_str());
	
    if (file.is_open()) {
        string line;
        while (getline(file, line)) {
            stringstream ss(line);
            string temp;

            RouterEntry entry;
            getline(ss, entry.type, ',');
            getline(ss, entry.xid, ',');
            getline(ss, entry.port, ',');
	    getline(ss, entry.nextHop, ',');
            getline(ss, entry.flags, ',');
	    printf("%s", line.c_str());

            add_to_table(router_table, entry);
        }
        file.close();
    } else {
	//will create an empty file if not existing
        cf = fopen(fname.c_str(), "w");
        if (cf != NULL) {
		cout<<"The empty forwardingtable file is created!"<<endl;
        } else {
  		cerr << "Error: Could not open file " << fname.c_str() << " for reading" << endl;
    		}
	}
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        cout << "Error: No input string provided!" << endl;
        return 1;
    }

    string arg1 = argv[1];
    string input_string = argv[3];
    string path = argv[2];
    stringstream ss(input_string);
    string temp;

    //
    XIAforwardingtable ftable;
    // Create a hash table for router entries
     unordered_map<string, RouterEntry> router_table= ftable.route_table;

    // Read the existing entries from file if arg1 is "-a"
    if (!path.empty() && (arg1 == "-a" || arg1 == "-r")) {
	    ftable.read_table_from_file(path, router_table);
    }

    // Parse the input string and update the router table
    while (getline(ss, temp, ',')) {
        RouterEntry entry;
        entry.type = temp;
        getline(ss, entry.xid, ',');
        getline(ss, entry.port, ',');
	getline(ss, entry.nextHop, ',');
        getline(ss, entry.flags, ',');

        if (arg1 == "-a") {
	    if (ftable.lookup_Route(router_table, entry.xid) != 0) {
            		ftable.add_to_table(router_table, entry);
	    } else {
		    auto _it = router_table.find(entry.xid);
            	    _it->second = entry; //just replace with the new routeData for existing xid
	    }
        } else if (arg1 == "-r") {
            ftable.delete_from_table(router_table, entry.xid);
        } 
    }

    // Write the updated table to file if arg1 is "-a" or "-r"
    if (arg1 == "-a" || arg1 == "-r") {
        ftable.write_table_to_file(path, router_table);
    }
    if (arg1 == "-s") {
	    //ftable.create_overlay_socket(path, input_string);

    }
    if (arg1 == "-i") {
	    cout<<"RZ create forwarding socket!!!!"<<endl;
            ftable.init_socket(path, input_string);
    }
    cout<<"print out the current routertable!!!!"<<endl;
    ftable.display_table(router_table);

    return 0;
}

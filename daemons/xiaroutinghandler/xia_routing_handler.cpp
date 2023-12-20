/*XIA Router Receiving Server*/

#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <vector>
#include <string.h>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include "dagaddr.hpp"
#include <memory>
#include <map>

using namespace std;

#define MSGBUFSIZE 1500
char c_msg[MSGBUFSIZE];

/*The function is to display the detailed protocol address info
 *@param Addrlabel: name given
 *@param sockaddr structure
 *Return: void
 */
void print_address(struct sockaddr* address, char* label)
{
        char hostname[256];

        const char *x = inet_ntop(address->sa_family,
                        (address->sa_family == AF_INET) ?
                        (void*)&(((struct sockaddr_in*)address)->sin_addr) :
                        (void*)&(((struct sockaddr_in6*)address)->sin6_addr),
                        hostname, sizeof(hostname));
        int port = (address->sa_family == AF_INET) ?
                ((struct sockaddr_in*)address)->sin_port :
                ((struct sockaddr_in6*)address)->sin6_port;
        port = ntohs(port);
	printf("Checklabel %s %s, port %d\n", label, x, port);
}

 /* The function is to lookup xiaforwarding data using the key xid 
  * and locate the nextHop IP for the requested XID if found a match
  *@param String router's forwardingtable datapath
  *@param:XID string to search
  *Return: nexthop ipaddress string from the routeentry if a match is found,
  * empty string otherwise
 */
std::string get_dest_addr_str(std::string fpath, std::string sid) {
        std::ifstream file_store(fpath);
        string line, s_dest("");

        if (file_store.is_open()){
                std::vector<string> data;
                while (getline(file_store, line)) //read entire line
                {
                        string tmp("");
                        cout << "check line: " << line << endl;
                        if (line.find(sid) != std::string::npos){
                                std::cout << "Found routingEntry destination dag: " << line << std::endl;
                                stringstream s(line); //build stringstream containing the xid
                                while(getline(s, tmp, ',')){ //split fields from line
                                        data.push_back(tmp);
                                        }
                                s_dest=data[3]; //only interested in nexthopIP
                        }
                }
                file_store.close();
        } else {
          printf("Failed to open forwardingroutes %s \n", fpath.c_str());
        }
return s_dest;
}

/* The function is to construct an interface mappings of router to use for setup listening 
 * sockets on router. The socket opened on router binds to router's specific interfaces.
 * @param: 
 * Return: map of router interfaces name index and IP
 * e.g.{(0, 172.65.1.1),(1, 172.65.4.1), (2, 172.65.2.1)}
 */
map<int, std::string> iface_socket_init(){
	map<int, string> overlay_sock;
	string ssocket;
	//open the iface file
	std::ifstream iface_f("./etc/overlay_socket.csv");
	std::string iface_l;
	if ( iface_f.is_open() ) {
		int index = 0;
		string temp;
		getline(iface_f, iface_l); //skip header
		while ( std::getline(iface_f, iface_l)) { 
			std::vector<string> if_data;
			stringstream stmp(iface_l);
			while(getline(stmp, temp, ',')) {
				if_data.push_back(temp); //interested in iface IP
			}
			overlay_sock[index] = if_data[1];
			++index;
		}
	iface_f.close();
	} else {
		std::cout << "Error: Failed to open overlay socket config file\n";
		}
	/*std::map<int, std::string>::iterator it = overlay_sock.begin();
	while (it != overlay_sock.end())
	{
		std::cout << "Key: " << it->first << ", Value: " << it->second << std::endl;
		  ++it;
	}*/

	return overlay_sock;
}


/* The function is to create UDP listening socket for each iterface associated to the router
 * @param mapping of router interface
 * Return: a data mapping of socket file descriptor and interface IP 
   */

map<std::string, int> create_sds_iface(std::map<int, string> map_iface) {
        struct addrinfo hints,*server;
        int r, sockfd;
        map<std::string, int> if_sock;
        std::map<int, string>::iterator it = map_iface.begin();
        char socket_if_lable[] = "IF socket";
        while (it != map_iface.end())
        {
                std::cout << "Key: " << it->first << ", Value: " << it->second << std::endl;
                //step1. server configuration
                memset(&hints, 0, sizeof(hints));       /* use memset_s() */
                hints.ai_family = AF_INET;                      /* IPv4 connection */
                hints.ai_socktype = SOCK_DGRAM;         /* UDP, datagram */
                r = getaddrinfo((it->second).c_str(), "8770", &hints, &server);
                if( r!=0 )
                {
                        perror("failed: get listening server address info");
                        exit(1);
                }

                //step2. open socket with domain,type and protocol
                sockfd = socket(server->ai_family,server->ai_socktype,server->ai_protocol);
                if( sockfd==-1 )
                {
                        perror("failed: create listening socket");
                        exit(1);
                }
                /* Eliminates "Address already in use" error from bind. */
                int optval = 1;
                if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,(const void *)&optval , sizeof(int))<0)
                        perror("setsockopt(SO_REUSEADDR) failed");

                #ifdef SO_REUSEPORT
                if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (const char*)&optval, sizeof(int)) < 0)
                        perror("setsockopt(SO_REUSEPORT) failed");
                #endif

		//set the sockeks as non-blocking
        	if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1){
			 perror("nonblocking option failed");
		}

		//step3. bind server to socket
                r = bind( sockfd,server->ai_addr,server->ai_addrlen);
                if( r==-1 )
                {
                        perror("failed: bind with server address");
                        exit(1);
                }
                 print_address((struct sockaddr *)server->ai_addr, socket_if_lable);
                 if_sock[it->second] =sockfd;
                 ++it;

		 freeaddrinfo(server);
        }

        std::map<std::string, int>::iterator its = if_sock.begin();
        while (its != if_sock.end())
        {
                std::cout<<its->first<< "value sd "<<its->second<<std::endl;
                ++its;
        }
        return if_sock;
}

/*The function is to forward the packet to nexthop from the specific interface
 *@param int socket_fd
 *@param string nexthop ipAddress:port
 *@param msghdr
 *return int numberofBytes forwarded
 */
int processing_forward_packet(int sockfd, std::string nexthop_str, struct msghdr msg){
	//now open udp sockets with this iface to connect nexthop IP:Port to send packet
	int rv, sd;
	int numbytes =0;
        struct addrinfo servaddr, *servinfo;
        struct sockaddr_in local_outip;
        char EndNodelabel[]= "R2 label:";

	//nexthop as transmit destination address	
	std::string nexthop_ip, nexthop_port;
	nexthop_ip= nexthop_str.substr(0,nexthop_str.find(":")); //only IP
	nexthop_port= nexthop_str.substr(nexthop_str.find(":")+1);
        printf("nexthop ip%s  port%s\n", nexthop_ip.c_str(), nexthop_port.c_str());

          	//create socket used to forward packet from current router to nexthop:
                memset(&servaddr, 0, sizeof servaddr); //empty the struct first
                memset(&local_outip, 0, sizeof(local_outip));
                servaddr.ai_family = AF_INET;
                servaddr.ai_socktype = SOCK_DGRAM;

		//store destination server information
		rv =  getaddrinfo(nexthop_ip.c_str(), nexthop_port.c_str(), &servaddr, &servinfo);
                if( rv!=0 )
                {
                        perror("getaddrinfo on server failed");
                        exit(1);
                } else{
			 print_address((struct sockaddr *)servinfo->ai_addr, EndNodelabel);
                }

                //bind the socket and forward packet to nexthop
                sd = socket(servinfo->ai_family,servinfo->ai_socktype,servinfo->ai_protocol);
                if( sd==-1 )
                {
                        perror("failed: create socket to send out packet to destIP");
                        exit(1);
                }

		local_outip.sin_family = AF_INET;
                local_outip.sin_addr.s_addr = inet_addr("172.65.2.1"); // the desired local IP
		local_outip.sin_port =htons(8770);
                bind(sd, (struct sockaddr*)&local_outip, sizeof(local_outip));

		//construct to send to nexthop as dest
		 msg.msg_name = (struct sockaddr_in *)servinfo->ai_addr;
    		 msg.msg_namelen = servinfo->ai_addrlen;
                 numbytes= sendmsg(sd, &msg, 0);
                        
		 printf("send out bytes %d succesfully! \n" , numbytes);

//        freeaddrinfo(servinfo);
	close(sd);
return numbytes;
}

/*The function is to test check the receiving packets on router's socket
 * test to use select to monitor receiving packets from different clients
 * then with the dest dag on packet/source addr to consult forwardingpath
 * last send packet to nexthop located in forwardingtable
 */

int check_processing_recv(int sockfd){
        int bytes_count;
        socklen_t fromlen;
        struct sockaddr fromaddr;
        char ipstr[1024], ipservice[20];
        fromlen = sizeof fromaddr;
        char buf[MSGBUFSIZE];

        bzero(buf, MSGBUFSIZE);
        bytes_count = recvfrom(sockfd, buf, sizeof buf, 0, &fromaddr, &fromlen);

        if (bytes_count <0) {
                perror("ERROR in recvfrom");
                return -1;
        }

        //get clientName  who sendto msg, and uses this later onforward
        printf("recv()'d %d bytes of data in buf\n", bytes_count);
        getnameinfo(&fromaddr, fromlen, ipstr, sizeof ipstr, ipservice, sizeof ipservice, NI_NUMERICHOST);
        printf("   fromclient: %s  %s %s \n", ipstr, ipservice, buf);

/*        bytes_sent=sendto(sockfd,buf,sizeof buf,0,&fromaddr,fromlen);
        if (bytes_sent <0)
        {
                perror("ERROR in sendto");
                return -1;
        }
        printf("sendto bytes: %d\n", bytes_sent);
*/
        return bytes_count;
        }

int processing_recv_packet(int sockfd){
	int retval=0, numbytes=0;
        std::string destTocheck("");
        //load xiaforwardingtable file to lookup
        std::string testpath ="./routingeng/forwardingtables/xiaforwardingdata.csv";

        //step4. obtain client input from recv function; &client is referred as clientAddr used for later nameinfo call
        ///r = recvfrom(sockfd,c_msg, maxBytes,0,&client,&client_size);

        //step5. process data to get the client name and sent response  --GOOD
        //NI_NUMERICHOST: as flag return IPAddress of the source clientAddr
        //getnameinfo(&client,client_size,output,maxBytes,0,0,NI_NUMERICHOST);
        //printf("receiving client IP %s size %d\n", output, r);

        //step6. check receiving packet to check where to forward
        //lookup route destination to see the path forwarding
        struct sockaddr_in r_addr;
        struct  msghdr msg;

        // We receive the XIA Header minus DAGs
        struct click_xia xiah;
        size_t xiah_len = sizeof(xiah) - sizeof(xiah.node);
        // and the DAGs along with payload
        uint8_t addrspluspayload[1532];
	uint8_t* c_buffer;

        struct iovec parts[2];
        parts[0].iov_base = &xiah;
        parts[0].iov_len = xiah_len;
        parts[1].iov_base = addrspluspayload;
        parts[1].iov_len = sizeof(addrspluspayload);

        msg.msg_name = &r_addr;
	msg.msg_namelen = sizeof(r_addr);
        msg.msg_iov = parts;
        msg.msg_iovlen = 2;
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        msg.msg_flags = 0;

        retval = recvmsg(sockfd, &msg, 0);
    if (retval > 0) {
        printf("Router recv: %d %lu %lu %d %d \n", retval, msg.msg_iov[0].iov_len, 
			msg.msg_iov[1].iov_len, xiah.nxt, xiah.ver);
        printf("from Addr:  %s:%d\n", inet_ntoa(r_addr.sin_addr),ntohs(r_addr.sin_port));
        if(xiah.ver != 1 || xiah.nxt != CLICK_XIA_NXT_DATA)
        {
                printf("ERROR:Invalid packet. Not XIA packet!!");
        }
        int payload_length = ntohs(xiah.plen);
	int payload_offset = sizeof(node_t) * (xiah.dnode + xiah.snode);
    	memcpy(c_buffer, &(addrspluspayload[payload_offset]), payload_length);
	std::string s( reinterpret_cast< char const* >(c_buffer) ) ;
    	//std::cout<<"check Msg sent: "<<s.c_str() << std::endl;
        printf("payload_length %d\n" , payload_length);
    }

    //step6. Now process the receiving packet:lookup routing table to find dest DAG from receiving
	Graph our_addr, their_addr;
        std::string  destTocheck_ip("");
        //int payload_length = ntohs(xiah.plen);
        const node_t* dst_wire_addr = (const node_t*)addrspluspayload;
        const node_t* src_wire_addr = dst_wire_addr + xiah.dnode;
        our_addr.from_wire_format(xiah.dnode, dst_wire_addr); //retrieve packet destAddr from dnode
        their_addr.from_wire_format(xiah.snode, src_wire_addr);//retrieve srcaddr that packet send from

	//cout<<"Check: reading from XIA header dest dag: "<< our_addr.dag_string().c_str()<<endl;
        //cout<<"Check: receiving from sourceAddr: "<<their_addr.dag_string().c_str()<<endl;

        Node my_aid = our_addr.intent_AID();
        std::string aid_str =my_aid.to_string();
        Node their_aid = their_addr.get_final_intent();

    /*Now start to check where to send out the packet*/
    //step7. lookup routing table for Dest type AID
       if (aid_str.size() < 20) {
                printf("ERROR: Intent AID invalid");
                return -1;
        }
        destTocheck = get_dest_addr_str(testpath, aid_str);
        if(!destTocheck.empty()){ //final step
                //printf("Check: AID nexthop: %s\n", destTocheck.c_str());
                //send nexthop
		 numbytes = processing_forward_packet(sockfd, destTocheck, msg);
		//printf("send packet to dest %d \n\n", retval);

        }else {
		printf("failed to locate xid route in %s\n\n", aid_str.c_str());
                //send to dest default gateway AD
                Node my_ad = our_addr.intent_AD();
                std::string ad_s(my_ad.to_string());
                destTocheck = get_dest_addr_str(testpath, ad_s);

		if (destTocheck.empty()){
                        printf("link issue. The router is the closest router to destination node!\n");
                        //wait for next cycle when AID routing is up on forwardingtable
                } else {
                	destTocheck_ip= destTocheck.substr(0,destTocheck.find(":")); //only IP
                	//printf("Check: AD nexthop IPAddr: %s\n", destTocheck_ip.c_str());

    			//step8. seperate process forwarding packet out to nexthop
                	//socket binded with specific output interface IP
                	numbytes= processing_forward_packet(sockfd, destTocheck, msg);
		}
        }
	std::cout<<"Check: number of bytes processed: " <<numbytes<<std::endl;
        //step6. clean-up and close
        //freeaddrinfo(server);
	//close(sockfd);
        return numbytes;
}

int main()
{
        int numByteRecv=0;
        //const int size = 1024;
	std::vector<int> sockIF_lst;
	int ret_val, max_sd=0;

	//step0. define select varaibles 
    	struct timeval tv;
	fd_set master_fds, read_fds;    // master fdset and temp working fdset for select()

    	FD_ZERO(&master_fds); //clear master and temp working sets
	FD_ZERO(&read_fds);

	tv.tv_sec = 20; // wait after 10s
    	tv.tv_usec = 0;

	//step1. add the sockets into master_fds; find the max fd
	//get all iterfaces on router
        map<int, string> test_ifaces;
        test_ifaces=iface_socket_init();
        for (auto& x: test_ifaces) {
            std::cout << "Iface index: "<< x.first << "Iface IP: " << x.second << '\n';
                }

	//add the sockets into fds_set for select
        map<string, int> iface_fds = create_sds_iface(test_ifaces);
        for (auto& t: iface_fds) {
            std::cout << "current max_sd: "<<max_sd<<" Iface IP" <<t.first << "on socket: " << t.second << '\n';
            sockIF_lst.push_back(t.second);
	    FD_SET(t.second , &master_fds);

	    //find the highest sd,to used it for select
            if(t.second > max_sd)
                      max_sd = t.second;
        }
	std::cout<<"Check the sd max number:"<<max_sd<<std::endl;
	//check the elements in master sets
        for (auto it : sockIF_lst) {
                std::cout <<"socket in sets" << it <<std::endl;
                }

        printf("UDP server is listening for client to send something......\n");

	while(true){
        	memcpy(&read_fds, &master_fds, sizeof(master_fds)); // work on a copy version of master

		ret_val= select(max_sd+1, &read_fds, NULL, NULL, &tv);
        	if (ret_val == -1) {
            		perror("failed on select()");
            		break;
		} 
		if (ret_val == 0) {     // timed out
		//	printf("Timeout occurred! No data after 20s.\n");
                }
	        if (ret_val >0) {
			std::cout<<"max_sd after select" <<max_sd<<std::endl;
			//now iterate each fd for ready for reading.
			for(int i = 0; i <= max_sd; i++) {	
				if(FD_ISSET(i, &read_fds)) {
           				/*test check number of bytes received 
					//printf("Check fd %d having data to read\n", i);
					int numCheck = check_processing_recv(i);
					printf("Number to check: %d\n", numCheck);
					*/
					  //  do {
						    numByteRecv=processing_recv_packet(i);
						if ( numByteRecv >0){
                                        		printf("dataBytes forwarded: %d on socket %d \n", numByteRecv, i);
                                		} else { // no data
                                        		if (numByteRecv ==0){
                                                		perror(" no more data to read \n");
                                        		} else {
                                                		if (errno != EWOULDBLOCK){
                                                        	perror("  recv() failed \n"); }	
                                        		}
                                   		        break; //next ready_fd
                                			} //
            				    //  } while (true);
				//	FD_CLR(i, &master_fds);
					}//end getting some data ready to read
				} //end looping through file descriptors
		} //get into successful select 
	} //end while 

    // Server ended. Return success
     //now cleanup all sockets
    for (auto it : sockIF_lst) {
            close(it);
                }
    return 0;
}

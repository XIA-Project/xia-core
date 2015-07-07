#include "Xsocket.h"
#include "chunks.h"

char s_ad[MAX_XID_SIZE];
char s_hid[MAX_XID_SIZE];

char my_ad[MAX_XID_SIZE];
char my_hid[MAX_XID_SIZE];


int initializeClient(const char *name)
{
	int sock, rc;
	sockaddr_x dag;
	socklen_t daglen;
	char sdag[1024];
	char IP[MAX_XID_SIZE];

    // lookup the xia service 
	daglen = sizeof(dag);
    if (XgetDAGbyName(name, &dag, &daglen) < 0)
      die(-1, "unable to locate: %s\n", name);


	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		 die(-1, "Unable to create the listening socket\n");
    
	if (Xconnect(sock, (struct sockaddr*)&dag, daglen) < 0) {
		Xclose(sock);
		 die(-1, "Unable to bind to the dag: %s\n", dag);
	}

	rc = XreadLocalHostAddr(sock, my_ad, MAX_XID_SIZE, my_hid, MAX_XID_SIZE, IP, MAX_XID_SIZE);

	if (rc < 0) {
		Xclose(sock);
		 die(-1, "Unable to read local address.\n");
	} else{
		warn("My AD: %s, My HID: %s\n", my_ad, my_hid);
	}
	
	// save the AD and HID for later. This seems hacky
	// we need to find a better way to deal with this
	Graph g(&dag);
	strncpy(sdag, g.dag_string().c_str(), sizeof(sdag));
//   	say("sdag = %s\n",sdag);
	char *ads = strstr(sdag,"AD:");
	char *hids = strstr(sdag,"HID:");
// 	i = sscanf(ads,"%s",s_ad );
// 	i = sscanf(hids,"%s", s_hid);
	
	if(sscanf(ads,"%s",s_ad ) < 1 || strncmp(s_ad,"AD:", 3) !=0){
		die(-1, "Unable to extract AD.");
	}
		
	if(sscanf(hids,"%s", s_hid) < 1 || strncmp(s_hid,"HID:", 4) !=0 ){
		die(-1, "Unable to extract AD.");
	}

	warn("Service AD: %s, Service HID: %s\n", s_ad, s_hid);
	return sock;
}


int main(void)
{
  int sock = initializeClient(CHUNKS_NAME);

  return 0;
}

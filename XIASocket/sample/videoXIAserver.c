/* XIA video server*/
/* Changes
 * used 1024 byte chunks
 * Runs for 1.5 min video, may also work with lower resolution video and bit longer
 *** Making it stateless
 * seems to have some issue currently ... for multiple clients
 * so first trying to do iterative with hacky way
*/

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include "Xsocket.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>




#define HID0 "HID:0000000000000000000000000000000000000000"
#define HID1 "HID:0000000000000000000000000000000000000001"
#define AD0   "AD:1000000000000000000000000000000000000000"
#define AD1   "AD:1000000000000000000000000000000000000001"
#define RHID0 "HID:0000000000000000000000000000000000000002"
#define RHID1 "HID:0000000000000000000000000000000000000003"
#define CID0 "CID:2000000000000000000000000000000000000001"
#define SID_VIDEO "SID:1f10000001111111111111111111111110000056"
#define DEBUG
#define TOTALPINGS 4000
#define MIGRATEPOINT 2000

#define CHUNKSIZE (1024)
using namespace std;


void *process_request (void *socketid);

vector<string> CIDlist;
	
int main(int argc, char *argv[])
{
	int sock, dlen, acceptSock, chunk_sock;
	unsigned char chunk[CHUNKSIZE];
	string fileName;
	pid_t pid;
	
	if(argc!=2)
	{
		error("no video file!\n");
		exit(1);
	}

	// Read from file and putCID	
	fileName=argv[1];
	int fid = open(fileName.c_str(), O_RDONLY);
	if(fid < 0){
		error("no video file!\n");
		exit(1);
	}	
	
	
	chunk_sock=Xsocket(XSOCK_CHUNK);
	
	sock=Xsocket(XSOCK_STREAM);
	if (sock < 0) {
		 error("Opening socket for putting content");
		 exit(-1);
	}
	
	//Make the sDAG (the one the server listens on)
	char * dag =(char*) malloc(snprintf(NULL, 0, "RE %s %s %s", AD1, HID1,SID_VIDEO) + 1);
    	sprintf(dag, "RE %s %s %s", AD1, HID1,SID_VIDEO);
   

 
	//Bind to the DAG
	Xbind(sock,dag);	

	int chunkIndex=0;
	while(1)
	{
		int size = read(fid, chunk, CHUNKSIZE);
		if(size<=0) break;
		
		// Name the CID
		// Should be generated from SHA hash (currently a hack)
		//TODO generate it from Sha hash
		stringstream ss;
		ss<<chunkIndex;
		string CID=ss.str();
		int CIDlen=CID.length();
		for(int i=0;i<40-CIDlen;i++)
			CID="0"+CID;
		CID="CID:"+CID;
		CIDlist.push_back(CID);
		//Make a CID DAG
    		char * cdag = (char*)malloc(snprintf(NULL, 0, "RE %s %s %s", AD1, HID1,CID.c_str()) + 1);
  		sprintf(cdag, "RE %s %s %s", AD1, HID1,CID.c_str()); 

		//cout << "putting CID " << cdag << " chunksize: " << size << "\n";

    		unsigned char* data=chunk;
    		XputCID(chunk_sock,data,size,0,cdag,strlen(cdag));
		chunkIndex++;
	}
	close(fid);	
	
	string CIDls="";
    	for(int i=0;i<CIDlist.size();i++)
    	{
		CIDls+=CIDlist[i];
		CIDls+="\t"; 
    	}
	string http_header = "HTTP/1.0 200 OK\r\nDate: Tue, 01 Mar 2011 06:14:58 GMT\r\nConnection: close\r\nContent-type: video/ogg\r\nServer: lighttpd/1.4.26\r\n\r\n";

	string message = http_header + CIDls;
	//cout << "Message is " << message << "\n";
		
	//cout << "length of message " << message.length() << "\n";
	// send http header
	// send list of CIDs 
	// the proxy should fetch each of these CID

    	while (1) {
		printf("\nListening...\n");
    		acceptSock = Xaccept(sock);
		printf("accept\n");
		
		// new thread will process the request
		pthread_t *process_thread = (pthread_t*)malloc(sizeof(pthread_t));
        	pthread_create(process_thread, NULL, process_request, (void *)&acceptSock);

	}
	
	Xclose(sock); // never reach here!
	
	
	return 0;
}



void *process_request (void *socketid) {
	int n;
	char SIDReq[1024];
	int *sock = (int*)socketid;
	int acceptSock = *sock; 
	
	
	// initalize
	for(int i = 0; i < 1024; i++)
		SIDReq[i] = '\0';
		
	//Receive packet
	n = Xrecv(acceptSock,SIDReq,1024,0);
		
	if(n>0) {
			
		string SIDReqStr(SIDReq);
		cout << "Got request: " << SIDReqStr << "\n";
		// if the request is about number of chunks return number of chunks
		// since this is first time, you would return along with header
		int found = SIDReqStr.find("numchunks");
			
		if(found != -1){
			//cout << " Request asks for number of chunks \n";
			stringstream yy;
			yy<<CIDlist.size();
			string cidlistlen = yy.str();
			Xsend(acceptSock,(void *) cidlistlen.c_str(), cidlistlen.length(), 0);
		} else {
			// the request would have two parameters
			// start-offset:end-offset
			int findpos = SIDReqStr.find(":");
			// split around this position
			string str = SIDReqStr.substr(0,findpos);
			int start_offset = atoi(str.c_str()); 
			str = SIDReqStr.substr(findpos + 1);
			int end_offset = atoi(str.c_str());

			// construct the string from CIDlist
			// return the list of CIDs
			string requestedCIDlist = "";
			// not including end_offset
			for(int i = start_offset; i < end_offset; i++){
				requestedCIDlist += CIDlist[i] + " ";
			}		
			Xsend(acceptSock, (void *)requestedCIDlist.c_str(), requestedCIDlist.length(), 0);
			//cout << "sending " << requestedCIDlist << "\n";
		}
       		n=0;
		for(int i = 0; i < 1024; i++)
			SIDReq[i] = '\0';
			
	}
	Xclose(acceptSock);
	pthread_exit(NULL);


}


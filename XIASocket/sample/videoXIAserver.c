/* XIA video server*/
/* Meant to run for one time service
TODO: change it to multiple thread model*/

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




#define HID0 "HID:0000000000000000000000000000000000000000"
#define HID1 "HID:0000000000000000000000000000000000000001"
#define AD0   "AD:1000000000000000000000000000000000000000"
#define AD1   "AD:1000000000000000000000000000000000000001"
#define RHID0 "HID:0000000000000000000000000000000000000002"
#define RHID1 "HID:0000000000000000000000000000000000000003"
#define CID0 "CID:2000000000000000000000000000000000000001"
#define SID0 "SID:0f00000000000000000000000000000000000055"
#define DEBUG
#define TOTALPINGS 4000
#define MIGRATEPOINT 2000

#define CHUNKSIZE (8192)
using namespace std;

int main(int argc, char *argv[])
{
	int sock, dlen, n;
	char SIDReq[1024];
	unsigned char chunk[CHUNKSIZE];
	string fileName;
	vector<string> CIDlist;
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
	sock=Xsocket();
	if (sock < 0) {
		 error("Opening socket for putting content");
		 exit(-1);
	}

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
    		char * cdag = (char*)malloc(snprintf(NULL, 0, "RE %s %s %s", AD0, HID0,CID.c_str()) + 1);
  		sprintf(cdag, "RE %s %s %s", AD0, HID0,CID.c_str()); 

		//cout << "putting CID " << cdag << " chunksize: " << size << "\n";

    		unsigned char* data=chunk;
    		XputCID(sock,data,size,0,cdag,strlen(cdag));
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
		
	//cout << "length of message " << message.length() << "\n";
	// send http header
	// send list of CIDs 
	// the proxy should fetch each of these CID
	
	sock=Xsocket();
	if (sock < 0) {
		error("Opening socket for service");
		exit(-1);
	}
	
	//Make the sDAG (the one the server listens on)
	char * dag =(char*) malloc(snprintf(NULL, 0, "RE %s %s %s", AD0, HID0,SID0) + 1);
    	sprintf(dag, "RE %s %s %s", AD0, HID0,SID0);
    
	// limit the message by 65521
	//message = message.substr(0, 8192);
	// find the last occurrence of CID
	//int rpos = message.rfind("CID");
	//message = message.substr(0,rpos);

	//cout << message << "\n";	

	//Bind to the DAG
	Xbind(sock,dag);
    	while (1) {
    	
		printf("\nListening...\n");
    		Xaccept(sock);
		printf("accept\n");

	
		// should send http header 
		// followed by list of CIDs
        
		//Receive packet
		n = Xrecv(sock,SIDReq,1024,0);
		if(n>0)
		{
			
			int last_loc = 0;
			while(last_loc < message.length()) {
				string submessage = message.substr(last_loc, 8192);
				int rpos = submessage.rfind("CID");
				submessage = submessage.substr(0,rpos);
				last_loc = last_loc + rpos;
				if(last_loc < message.length())
					submessage = submessage + " more";
				Xsend(sock, (void *) submessage.c_str(), submessage.length(), 0);
		
				Xrecv(sock, SIDReq, 1024, 0);
			}
		
		

			//Xsend(sock, (void *) info.c_str(), info.length(), 0);
			//Xrecv(sock, SIDReq, 1024, 0);
			// get ack

			// send that many messages
			//int num_iter = (message.length()/65521)+1; 

			//cout << "Sending CID list \n";
			
			//for(int i = 0; i < 1; i++){
			//	int st_pos = i * 65521;
			//	int end_pos = (i+1) * 8192;
			//	if(end_pos > message.length()) {
			//		end_pos = message.length()+1;
			//	}
			//	int diff = (end_pos - st_pos);
			//	printf(" length of message %d\n", diff);
			//	string mstr = message.substr(st_pos, end_pos);
				
			//	Xsend(sock, (void *) mstr.c_str(), diff, 0);
				// get ack
			//	Xrecv(sock, SIDReq, 1024, 0);
			//}
			
			/*
		        stringstream yy;
			yy<<CIDlist.size();
			string cidlistlen = yy.str();
			Xsend(sock,(void *) cidlistlen.c_str(), cidlistlen.length(), 0);
		
			for(int i = 0; i < CIDlist.size(); i++){
				Xrecv(sock, SIDReq, 1024, 0);
				Xsend(sock, (void *)CIDlist[i].c_str(), CIDlist[i].length(), 0);
				cout << "sending " << CIDlist[i] << "\n";

			}*/	
       			n=0;
	    	}	
	}
	return 0;
}


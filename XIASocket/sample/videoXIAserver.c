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

#define CHUNKSIZE 8192
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
	
	sock=Xsocket();
	if (sock < 0) {
		error("Opening socket for service");
		exit(-1);
	}
	
	//Make the sDAG (the one the server listens on)
	char * dag =(char*) malloc(snprintf(NULL, 0, "RE %s %s %s", AD0, HID0,SID0) + 1);
    	sprintf(dag, "RE %s %s %s", AD0, HID0,SID0);
    
	//Bind to the DAG
	Xbind(sock,dag);
    	while (1) {
    	
		printf("\nListening...\n");
    		Xaccept(sock);
		printf("accept\n");
        
		//Receive packet
		n = Xrecv(sock,SIDReq,1024,0);
		if(n>0)
		{
		        stringstream yy;
			yy<<CIDlist.size();
			string cidlistlen = yy.str();
			Xsend(sock,(void *) cidlistlen.c_str(), cidlistlen.length(), 0);
		
			for(int i = 0; i < CIDlist.size(); i++){
				Xrecv(sock, SIDReq, 1024, 0);
				Xsend(sock, (void *)CIDlist[i].c_str(), CIDlist[i].length(), 0);
				cout << "sending " << CIDlist[i] << "\n";

			}	
       			n=0;
	    	}	
	}
	return 0;
}


/***
author: Wenfei 
This is for testing putCID functionality
**/

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
#define HID0 "HID:0000000000000000000000000000000000000000"
#define HID1 "HID:0000000000000000000000000000000000000001"
#define AD0   "AD:1000000000000000000000000000000000000000"
#define AD1   "AD:1000000000000000000000000000000000000001"
#define RHID0 "HID:0000000000000000000000000000000000000002"
#define RHID1 "HID:0000000000000000000000000000000000000003"
#define CID0 "CID:2000000000000000000000000000000000000001"
#define SID0 "SID:0f00000000000000000000000000000000000055"
#define IDLEN 40

#define CHUNKSIZE 1024
using namespace std;

int main(int argc, char *argv[])
{
	int sock;
	char chunk[CHUNKSIZE];
	string fileName;
	if(argc!=2)
	{
		error("no video file! run as: ./pCIDsl large_video_file\n");
		exit(1);
	}
	fileName=argv[1];
	sock=Xsocket();
	if (sock < 0) error("Opening socket");
	
	FILE* file=fopen(fileName.c_str(),"rb");
	if(file==NULL)
	{
		error("no file!\n");
		exit(1);
	}	
	int chunkIndex=0;
	while(1)
	{
		int size=fread(chunk,1, CHUNKSIZE, file);
		if(size==0) break;

		// Construct CID
		stringstream ss;
		ss<<chunkIndex;
		string CID=ss.str();
		int CIDlen=CID.length();
		for(int i=0;i< IDLEN - CIDlen;i++)
			CID="0"+CID;
		CID="CID:"+CID;
    		char * cdag = (char*)malloc(snprintf(NULL, 0, "RE %s %s %s", AD0, HID0,CID.c_str()) + 1);
  		sprintf(cdag, "RE %s %s %s", AD0, HID0,CID.c_str()); 
    		char* data=chunk;

		//Call XputCID
    		XputCID(sock,data,size,0,cdag,strlen(cdag));
		chunkIndex++;
	}
	Xclose(sock);
	return 0;
}

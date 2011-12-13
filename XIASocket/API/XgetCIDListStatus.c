/*
** Copyright 2011 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/*
* GetCIDLISTStatus
*/

#include "Xsocket.h"
#include "Xinit.h"

// Called after XgetCID(), it checks whether each of the requested CIDs is waiting to be read or still on the way
// int status in cDAGvec struct will be set by this call
// Return value: 1: (all) waiting to be read, 0: (any) waiting for chunk response, -1: failed 

int XgetCIDListStatus(int sockfd, struct cDAGvec *cDAGv, int numCIDs)
{
        
	char buffer[2048];
	struct sockaddr_in their_addr;
	socklen_t addr_len;
	char statusbuf[2048];
	
	struct addrinfo hints, *servinfo,*p;
	int rv;
	int numbytes;
	const char *buf="CID list request status query";//Maybe send more useful information here.
	size_t cdagListsize = 0;
	int status = WAITING_FOR_CHUNK;
	
	for (int i=0; i< numCIDs; i++) {
		cdagListsize += (cDAGv[i].dlen + 1);
	}
	char * cdagList = (char *) malloc (cdagListsize + 1);
    	memset (cdagList, 0, cdagListsize);
    	
    	strcpy(statusbuf, "WAITING");
    	
    	strcpy(cdagList, cDAGv[0].cDAG);
    	for (int i=1; i< numCIDs; i++) {
    		strcat(cdagList, "^");
		strcat(cdagList, cDAGv[i].cDAG);
		
		strcat(statusbuf, "^");
		strcat(statusbuf, "WAITING");
	}


	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;


	if ((rv = getaddrinfo(CLICKDATAADDRESS, CLICKDATAPORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	p=servinfo;

	// protobuf message
	xia::XSocketMsg xia_socket_msg;

	xia_socket_msg.set_type(xia::XGETCIDSTATUS);

	xia::X_Getcidstatus_Msg *x_getcidstatus_msg = xia_socket_msg.mutable_x_getcidstatus();
  
  	x_getcidstatus_msg->set_numcids(numCIDs);
	x_getcidstatus_msg->set_cdaglist(cdagList);
	x_getcidstatus_msg->set_status_list(statusbuf);
	x_getcidstatus_msg->set_payload((const char*)buf, strlen(buf)+1);

	std::string p_buf;
	xia_socket_msg.SerializeToString(&p_buf);

	numbytes = sendto(sockfd, p_buf.c_str(), p_buf.size(), 0, p->ai_addr, p->ai_addrlen);
	freeaddrinfo(servinfo);

	if (numbytes == -1) {
		perror("XXgetCIDListStatus(): XgetCIDListStatus failed");
		return(-1);
	}

     	 
       //Process the reply
        addr_len = sizeof their_addr;
        if ((numbytes = recvfrom(sockfd, buffer, MAXBUFLEN-1 , 0,
                                        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
                        perror("XgetCIDListStatus(): recvfrom");
                        return -1;
        }

	//protobuf message parsing
	xia::XSocketMsg xia_socket_msg1;
	xia_socket_msg1.ParseFromString(buffer);

	if (xia_socket_msg1.type() == xia::XGETCIDSTATUS) {
		
		    xia::X_Getcidstatus_Msg *x_getcidstatus_msg1 = xia_socket_msg1.mutable_x_getcidstatus();
		    strcpy(statusbuf,  x_getcidstatus_msg1->status_list().c_str()); 
		    
		    char status_tmp[100];
		    char * start = statusbuf;
		    char * pch;
		    int status_for_all = READY_TO_READ;
		    
		    for (int i=0; i< numCIDs; i++) {
		    	pch = strchr(start,'^');
		    	if (pch!=NULL) {
		    		// there's more CID status followed
		    		strncpy (status_tmp, start, pch - statusbuf);
		    		status_tmp[pch - start]='\0';
		    		start = pch+1;
		    	
		    	} else {
		    		// this is the last CID status in this batch.
		    		strcpy (status_tmp, start);
		    	}
		    	
			if (strcmp(status_tmp, "WAITING") == 0) {
				cDAGv[i].status = WAITING_FOR_CHUNK;
				
				if (status_for_all != REQUEST_FAILED) { 
					status_for_all = WAITING_FOR_CHUNK;
		    		}
		    		
		    	} else if (strcmp(status_tmp, "READY") == 0) {
		    		cDAGv[i].status = READY_TO_READ;
		    	
		    	} else if (strcmp(status_tmp, "FAILED") == 0) {
		    		cDAGv[i].status = REQUEST_FAILED;
		    		status_for_all = REQUEST_FAILED;
		    	
		    	} else {
		    		cDAGv[i].status = REQUEST_FAILED;
		    		status_for_all = REQUEST_FAILED;
		    	}
		    	
		    }
		    
		    return status_for_all;
		    
	}
	
        return REQUEST_FAILED; 
      
    
}





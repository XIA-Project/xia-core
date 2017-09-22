#include "stage_utils.h"
#include <time.h>

using namespace std;

bool stage = true;
char fin[256];
int ftpSock;
XcacheHandle h;

int getFile(int sock)
{
    FILE *fd = fopen(fout, "w");
    ofstream logFile("clientTime.log");
    int n;
    int status = -1;
    char send_to_server_[XIA_MAX_BUF];
    memset(send_to_server_, '\0', strlen(send_to_server_));
    sprintf(send_to_server_, "finish receiveing the current CIDs.");

    int chunkSock;
    if ((chunkSock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
        die(-1, "unable to create chunk socket\n");
    }

    char cmd[XIA_MAX_BUF];
    char reply[XIA_MAX_BUF];

    vector<string> CIDs;

    // send the file request to the xftp server
    sprintf(cmd, "get %s", fin);
    sendStreamCmd(sock, cmd);
    int fetchTime = 0;

    // receive the CID list from xftp server
    while (1) {
        say("In getFile, receive CID list from xftp server.\n");
        memset(reply, '\0', XIA_MAX_BUF);
        if ((n = Xrecv(sock, reply, sizeof(reply), 0)) < 0) {
            Xclose(sock);
            die(-1, "Unable to communicate with the server\n");
        }
        if (strncmp(reply, "cont", 4) == 0) {
            say("Receiving partial chunk list\n");
            char reply_arr[strlen(reply)];
            //char reply_arr[strlen(reply+5)];
            strcpy(reply_arr, reply + 5);
            char *cid;
            cid = strtok(reply_arr, " ");
            CIDs.push_back(cid);
say("---------CID:%s\n",cid);
            // register CID
            while ((cid = strtok(NULL, " ")) != NULL) {
                CIDs.push_back(cid);
say("---------CID:%s\n",cid);
            }
        }

        else if (strncmp(reply, "done", 4) == 0) {
            say("Finish receiving all the CIDs\n");
            break;
        }
        sayHello(sock, send_to_server_);
    }

    XcacheHandleInit(&h);

    vector<unsigned int> chunkSize;
    vector<long> latency;
    vector<long> chunkStartTime;
    vector<long> chunkFinishTime;

    long startTime = now_msec();
    unsigned int bytes = 0;

    // chunk fetching begins
    //char data[CHUNKSIZE];
	void *data;
    logFile << now_msec() << "	" << 0 <<endl;
    for (unsigned int i = 0; i < CIDs.size(); i++) {

        say("The number of chunks is %d.\n", CIDs.size());
        say("Fetching chunk %u / %lu\n", i, CIDs.size() - 1);
        int len;
        sockaddr_x addr;
        if (stage) {
            memset(cmd, 0, sizeof(cmd));
            sprintf(cmd, "fetch");
            sprintf(cmd, "%s %s", cmd, CIDs[i].c_str());
            say("CMD is :%s\n", cmd);
            if (send(stageManagerSock, cmd, strlen(cmd), 0) < 0) {
                die(-1, "send cmd fail! cmd is %s", cmd);
            }
            say("After send Fetch!\n");
            memset(cmd, 0, sizeof(cmd));
            if ((len = recv(stageManagerSock, cmd, XIA_MAX_BUF, 0)) < 0) {
                die(-1, "fail to recv from stageManager!");
            }
            say("Get NewDag: %s\n", cmd);
			Graph g(cmd);
			g.fill_sockaddr(&addr);
        }
        else {
			Graph g((char*)CIDs[i].c_str());
			g.fill_sockaddr(&addr);
			
        say("CID=%s %d\n",(char*)CIDs[i].c_str(), CIDs[i].size());
        }
        long start_time = now_msec();
		int retry=5;
		while(retry--){
			if ((len = XfetchChunk(&h, &data, XCF_BLOCK, &addr, sizeof(addr))) < 0) {		
				say("XfetchChunk Failed, retrying");
			}
			else break;
		}
		if(len < 0)
			die(-1, "XfetchChunk Failed\n");
        long end_time = now_msec();
        char req[256];
        //dag_to_url(req,256,&addr);
		Graph g(&addr);
		g.fill_sockaddr(&addr);
		strcpy(req, g.http_url_string().c_str());
		
        fetchTime = end_time - start_time;
		if(stage){
			memset(cmd, 0, sizeof(cmd));
			sprintf(cmd, "time");
			//sscanf(cmd, "time %ld", &timeWifi);
			sprintf(cmd, "%s %ld", cmd, fetchTime);
			say("CMD is :%s\n", cmd);
			if (send(stageManagerSock, cmd, strlen(cmd), 0) < 0) {
				die(-1, "send cmd fail! cmd is %s", cmd);
			}
			if ((recv(stageManagerSock, cmd, XIA_MAX_BUF, 0)) < 0) {
                die(-1, "fail to recv from stageManager!");
            }
		}
		logFile << end_time << "	" << i+1 <<endl;
        say("writing %d bytes of chunk %s to disk\n", len, string2char(CIDs[i]));
        fwrite((char*)data, 1, len, fd);
        bytes += len;
        chunkSize.push_back(len);
    }
	free(data);
    fclose(fd);
    long finishTime = now_msec();
    logFile << "Received file " << fout << " at "<< (1000 * (float)bytes / 1024) / (float)(finishTime - startTime) << " KB/s (Time: " << finishTime - startTime << " msec, Size: " << bytes << "bytes)\n";
	printf("Total using time: %d\n", finishTime - startTime);
    sendStreamCmd(sock, "done");    // chunk fetching ends, send command to server
    Xclose(chunkSock);
    Xclose(sock);
    return status;
}

int main(int argc, char **argv)
{
    while (1) {
        char myAD[MAX_XID_SIZE];
        char myHID[MAX_XID_SIZE];

        char ftpServAD[MAX_XID_SIZE];
        char ftpServHID[MAX_XID_SIZE];
        say("In main function of xftp_adv_client.\n");
        if (argc == 2) {
            strcpy(fin, argv[1]);
            sprintf(fout, "my%s", fin);

            ftpSock = initStreamClient(getXftpName(), myAD, myHID, ftpServAD, ftpServHID);
            getFile(ftpSock);
            return 0;
        }
        else {
            die(-1, "Please input the filename as the second argument\n");
        }
    }

    return 0;
}

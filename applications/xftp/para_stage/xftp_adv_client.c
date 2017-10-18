#include "stage_utils.h"
#include <time.h>


#define VERSION "v1.0"
#define TITLE "XIA Advanced FTP client"

using namespace std;

bool stage = true;

char fin[256], fout[256];

int ftpSock, stageManagerSock;

XcacheHandle h;
vector<string> CIDs;

FILE *fd = fopen(fout, "w");
ofstream logFile("clientTime.log");

int curChunk=0;
int parallelChunk=6;
pthread_mutex_t pro_curChunk = PTHREAD_MUTEX_INITIALIZER;
int writeChunk=0;
pthread_mutex_t pro_write = PTHREAD_MUTEX_INITIALIZER;

void getConfig(int argc, char** argv)
{
        int c;
        opterr = 0;

        while ((c = getopt(argc, argv, "f:p:")) != -1) {
                switch (c) {
                        //file name
                        case 'f':
                            strcpy(fin, optarg);
                            break;
                        //percentage
                        case 'p':
                            if ((parallelChunk = atoi(optarg)) == 0) {
                                die(-1, "Wrong Parameter\n");
                            }
                            break;    
                        default:
                            break;
                }
        }
}

int parallel_getchunk(void * idx){
    void * data;
    int len;
    int index = *((int* )idx)
    sockaddr_x addr;
    char cmd[XIA_MAX_BUF];
    Graph g1((char *)CID);
    g1.fill_sockaddr(&addr);
    string CID = CIDs[index];
    long start_time = now_msec();
    int retry=5;
    while(retry--){
        say("before fetch a chunk");
        if ((len = XfetchChunk(&h, &data, XCF_BLOCK, &addr, sizeof(addr))) < 0) {       
            say("XfetchChunk Failed, retrying");
        }
        else break;
    }
    say("after fetch a chunk");
    if(len < 0)
        die(-1, "XfetchChunk Failed\n");
    long end_time = now_msec();
    char req[256];
    //dag_to_url(req,256,&addr);
    Graph g(&addr);
    g.fill_sockaddr(&addr);
    strcpy(req, g.http_url_string().c_str());
    
    fetchTime = end_time - start_time;
    logFile << end_time << "    " << i+1 <<endl;

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
    pthread_mutex_lock(pro_curChunk);
    curChunk -= 1;
    pthread_mutex_unlock(pro_curChunk);
    //TODO:add writing to files
    //say("writing %d bytes of chunk %s to disk\n", len, string2char(CIDs[i]));

    while(1){
        if(writeChunk == index)
            break;
    }
    fwrite((char*)data, 1, len, fd);
    pthread_mutex_lock(pro_write);
    writeChunk += 1;
    pthread_mutex_unlock(pro_write);
}

int getFile(int sock)
{
    
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

    // update CID list to the local staging service if exists.
    // Send CID list to stage manager
    if (stage) {
        if ((n = updateManifest(stageManagerSock, CIDs) < 0)) {
            close(stageManagerSock);
            die(-1, "Unable to communicate with the local prefetching service\n");
        }
        say("After updateManifest\n");
    }
    XcacheHandleInit(&h);

    vector<unsigned int> chunkSize; // in bytes
    vector<long> latency;
    vector<long> chunkStartTime;
    vector<long> chunkFinishTime;

    long startTime = now_msec();
    unsigned int bytes = 0;

    // chunk fetching begins
    //char data[CHUNKSIZE];
	void *data;// = (char*)malloc(CHUNKSIZE);
logFile << now_msec() << "	" << 0 <<endl;
    for (unsigned int i = 0; i < CIDs.size(); i++) {
        say("The number of chunks is %d.\n", CIDs.size());
        say("Fetching chunk %u / %lu\n", i, CIDs.size() - 1);

        while(1){
            if(curChunk < parallelChunk)
                break;
        }
        pthread_mutex_lock(pro_curChunk);
        curChunk += 1;
        pthread_mutex_unlock(pro_curChunk);

        pthread_t thread_parallel_getchunk;
        pthread_create(&thread_parallel_getchunk, NULL, parallel_getchunk, (void *)(string2char(CIDs[i]));
		
        //------------//logFile << i <<" Chunk. Running time is: " << end_time - start_time << " ms. req: " << req << endl;
		//logFile << end_time << "	" << i+1 <<endl;
        //bytes += len;
        //chunkSize.push_back(len);
        //latency.push_back(end_time - start_time);
        //chunkStartTime.push_back(start_time);
        //chunkFinishTime.push_back(now_msec());
    }
	free(data);
    fclose(fd);
    long finishTime = now_msec();
    logFile << "Received file " << fout << " at "<< (1000 * (float)bytes / 1024) / (float)(finishTime - startTime) << " KB/s (Time: " << finishTime - startTime << " msec, Size: " << bytes << "bytes)\n";
	printf("Total using time: %d\n", finishTime - startTime);
    sendStreamCmd(sock, "done");    // chunk fetching ends, send command to server
    //for (unsigned int i = 0; i < CIDs.size(); i++) {
    //   logFile << fout << "\t" << CIDs[i] << "\t" << chunkSize[i] << " B\t" << latency[i] << "\t" << chunkStartTime[i] << "\t" << chunkFinishTime[i] << endl;
    //}
    Xclose(chunkSock);
    Xclose(sock);
    return status;
}

int main(int argc, char **argv)
{
    getConfig(argc, argv);
    char myAD[MAX_XID_SIZE];
    char myHID[MAX_XID_SIZE];

    char ftpServAD[MAX_XID_SIZE];
    char ftpServHID[MAX_XID_SIZE];
    say("In main function of xftp_adv_client.\n");
    sprintf(fout, "my%s", fin);
    ftpSock = initStreamClient(getXftpName(), myAD, myHID, ftpServAD, ftpServHID);

    stageManagerSock = registerUnixStageManager(UNIXMANAGERSOCK);

    say("The current stageManagerSock is %d\n", stageManagerSock);
    if (stageManagerSock < 0) {
        say("No local staging service running: %d\n", stageManagerSock);
        stage = false;
    }
    getFile(ftpSock);
    close(stageManagerSock);
    return 0;
}

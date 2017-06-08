#include "stage_utils.h"

using namespace std;
string lastSSID, currSSID, lastSSID2, currSSID2;
//TODO: better to rename in case of mixing up with the chunkProfile struct in server   --Lwy   1.16
struct chunkProfile {
    int state;
    string dag;
    long stageStartTimestemp;
    long stageFinishTimestemp;

    chunkProfile(string _dag = "", int _state = BLANK): state(_state), dag(_dag) {}
};

int rttWifi = -1, rttInt = -1;
long timeWifi = 0, timeInt = 0;
int chunkToStage = 3, alreadyStage = 0, pendingStage = 0;

vector<string> allDAGs;
map<string, chunkProfile> CIDToProfile;
//map<int, int> fetchIndex;
int fetchIndex;

bool netStageOn = true;
int thread_c = 0;
int HANDOFFTIME = 4;
int HANDOFFPOLICY = 1;
// TODO: this is not easy to manage in a centralized manner because we can have multiple threads for staging data due to mobility and we should have a pair of mutex and cond for each thread
pthread_mutex_t profileLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dagVecLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t stageMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t StageControl = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t preStageControl = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fetchLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t chunkMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t responseMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t stopMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t handoffModeMutex = PTHREAD_MUTEX_INITIALIZER;
int stopFlag = 0;

pthread_cond_t handoffCond=PTHREAD_COND_INITIALIZER;
int handoffMode = 0;
pthread_mutex_t fetchFlagMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t handoffFlagMutex = PTHREAD_MUTEX_INITIALIZER;

int CONNECT_TIME = 7000;
int SOFTSTAGE_TIME = 1000;
int OVERLAP_TIME = 2000;
int DISCONNECT_TIME = 0;
int netsize = 1;
int FREQ[2]={2457, 2432};
pthread_mutex_t encounterTime = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t disconnectTime = PTHREAD_MUTEX_INITIALIZER;

long timeStamp;
ofstream connetTime("connetTime.log");
ofstream windowToStage("windowToStage.log");
ofstream managerTime("managerTime.log");

void getConfig(int argc, char** argv)
{
    int c;
    opterr = 0;
    while ((c = getopt(argc, argv, "h:c:d:n:s:f:F:o:")) != -1) {
        switch (c) {
			case 'c':
				CONNECT_TIME = atoi(optarg);
				break;
			case 'o':
				OVERLAP_TIME = atoi(optarg);
				break;
			case 'd':
				DISCONNECT_TIME = atoi(optarg);
				break;
			case 's':
				SOFTSTAGE_TIME = atoi(optarg);
				break;
			case 'n':
				netsize = atoi(optarg);
				break;
            case 'h':
				HANDOFFPOLICY = atoi(optarg);
				break;
			case 'f':
				FREQ[0] = atoi(optarg);
				break;
			case 'F':
				FREQ[1] = atoi(optarg);
            default:
                    break;
    	}
    }
}

//Update the stage window
void updateStageArg() {
    if (rttWifi != -1 && rttInt != -1 && timeWifi && timeInt) {
        int left = timeWifi + rttWifi;//rttWifi是stage_manager和stage_server; timeWifi是client下载一个chunk的时间
        int right = timeInt + rttWifi + rttInt;//rttInt是stage_server到content_server; timeInt是stage一个chunk要花的时间
        chunkToStage = static_cast<double>(right) / left + 1;
    }
    else {
        chunkToStage = 3;
    }
    windowToStage << "rttWifi: " << rttWifi
                  << " rttInt: " << rttInt
                  << " timeWifi: " << timeWifi
                  << " timeInt: " << timeInt 
	          << " chunkToStage: " << chunkToStage << endl;
}
// TODO: mem leak; window pred alg; int netMonSock;
void regHandler(int sock, char *cmd)
{
    say("In regHandler. CMD: %s\n", cmd);
    if (strncmp(cmd, "reg cont", 8) == 0) {
        say("Receiving partial chunk list\n");
        char cmd_arr[strlen(cmd + 9)];
        strcpy(cmd_arr, cmd + 9);
        char *dag, *saveptr;
        pthread_mutex_lock(&dagVecLock);
        pthread_mutex_lock(&profileLock);
        dag = strtok_r(cmd_arr, " ", &saveptr);

        allDAGs.push_back(dag);
        CIDToProfile[dag] = chunkProfile(string(dag), BLANK);
        while ((dag = strtok_r(NULL, " ", &saveptr)) != NULL) {
            allDAGs.push_back(dag);
            CIDToProfile[dag] = chunkProfile(dag, BLANK);
            say("DAG: %s\n", dag);
        }
        say("++++++++++++++++++++++++++++++++The remoteSID is %d\n", sock);
        pthread_mutex_unlock(&profileLock);
        pthread_mutex_unlock(&dagVecLock);
        return;
    }

    else if (strncmp(cmd, "reg done", 8) == 0) {
        pthread_mutex_lock(&stageMutex);
        fetchIndex = 0;
        pthread_mutex_unlock(&stageMutex);
        pthread_mutex_unlock(&StageControl);
		//pthread_mutex_unlock(&preStageControl);
        say("Receive the reg done command\n");
        return;
    }
}

int delegationHandler(int sock, char *cmd)
{
    say("In delegationHandler.\nThe command is %s\n", cmd);

    string tmp;
    pthread_mutex_lock(&dagVecLock);
//say("In delegationHandler.\nThe command is %s\n", cmd);
    auto iter = find(allDAGs.begin(), allDAGs.end(), cmd);
    if (!netStageOn || iter == allDAGs.end()) { // not register
        tmp = cmd;
        pthread_mutex_unlock(&dagVecLock);
    }
    else {
        auto dis = distance(allDAGs.begin(), iter);
        pthread_mutex_unlock(&dagVecLock);
        //allDAGs.erase();
        while (true) {
say("waiting for pending\n");
say("In delegationHandler while.\nThe command is %s\n", cmd);
            pthread_mutex_lock(&profileLock);
            if (CIDToProfile[cmd].state == READY) {
                //CIDToProfile[cmd].state = IGNORE;
				alreadyStage--;
                break;
                say("DAG: %s change into IGNORE!\n", cmd);
            }
			/*if(CIDToProfile[cmd].state == BLANK){
	    	    CIDToProfile[cmd].state = READY;
				//pthread_mutex_unlock(&fetchLock);
				break;
            //    CIDToProfile[cmd].dag = cmd;
            }*/
            pthread_mutex_unlock(&profileLock);
        }

        tmp = CIDToProfile[cmd].dag;
        //CIDToProfile.erase(cmd);
        pthread_mutex_unlock(&profileLock);

        pthread_mutex_lock(&stageMutex);
        fetchIndex = dis;
        pthread_mutex_unlock(&stageMutex);
		pthread_mutex_unlock(&preStageControl);
		pthread_mutex_unlock(&StageControl);
    }
//say("In delegationHandler. send\nThe command is %s\n", cmd);
	if(HANDOFFPOLICY == 1 || HANDOFFPOLICY ==2){
		pthread_mutex_lock(&handoffModeMutex);
		if(handoffMode == 2) pthread_cond_wait(&handoffCond, &handoffModeMutex);
		handoffMode = 1;
		pthread_mutex_unlock(&handoffModeMutex);
	}
    if (send(sock, tmp.c_str(), tmp.size(), 0) < 0) {
        warn("socket error while sending data, closing connetTime\n");
        return -1;
    }
	
    //say("End of delegationHandler\n");
    return 0;
}

void *clientCmd(void *socketid)
{
    char cmd[XIA_MAXBUF];
    int sock = *((int*)socketid);
    int n;

    pthread_mutex_lock(&stageMutex);
    fetchIndex = 0;
    pthread_mutex_unlock(&stageMutex);
    while (1) {
        memset(cmd, 0, sizeof(cmd));
        if ((n = recv(sock, cmd, sizeof(cmd), 0)) < 0) {
            warn("socket error while waiting for data, closing connetTime\n");
            break;
        }
        else {
            if (n == 0) {
                say("Unix Socket closed by client.");
                break;
            }
        }
        say("clientCmd````````````````````````````````Receive the command that: %s\n", cmd);
        // registration msg from xftp client: reg DAG1, DAG2, ... DAGn
        if (strncmp(cmd, "reg", 3) == 0) {
            say("Receive a chunk list registration message\n");
            regHandler(sock, cmd);
            //pthread_mutex_unlock(&StageControl);
        }
        else if (strncmp(cmd, "fetch", 5) == 0) {
            say("Receive a chunk request\n");
			//pthread_mutex_lock(&fetchLock);
	    	pthread_mutex_unlock(&StageControl);
            char dag[256] = ""; 
            sscanf(cmd, "fetch %s Time:%ld" ,dag, &timeWifi);
            if (delegationHandler(sock, dag) < 0) {
                break;
            }
        }
        else if (strncmp(cmd, "time", 4) == 0) {
			if(HANDOFFPOLICY == 1 || HANDOFFPOLICY == 2 ){
				pthread_mutex_lock(&handoffModeMutex);
				if(handoffMode == 1)handoffMode = 0;
				pthread_mutex_unlock(&handoffModeMutex);
				pthread_cond_signal(&handoffCond);
			}
            //say("Get Time! cmd: %s\n", cmd);
            sscanf(cmd, "time %ld", &timeWifi);
            updateStageArg();
        }
        //usleep(SCAN_DELAY_MSEC*1000);
    }
    close(sock);
    say("Socket closed\n");
    //pthread_mutex_lock(&profileLock);
    //CIDToProfile.erase(sock);
    //pthread_mutex_unlock(&profileLock);
    //pthread_mutex_lock(&dagVecLock);
    //allDAGs.erase(sock);
    //pthread_mutex_unlock(&dagVecLock);
    pthread_exit(NULL);
}
void *stageChunk(void * lastSock){
//say("*********************************Before while loop of stageChunk\n");
	//pthread_mutex_lock(&chunkMutex);
	int netStageSock = ((int*)lastSock)[0];
	int needStage = ((int*)lastSock)[1];
	int chunkToRecv = ((int*)lastSock)[2];
	char reply[XIA_MAX_BUF] = {0};
	int cnt=0;
	while(1) {
		//if(chunkToRecv <= cnt) continue;
		//pthread_mutex_lock(&responseMutex);
		//sayHello(netStageSock, "READY TO RECEIVE!\n");
		memset(reply,0,sizeof(reply));
//say("*********************************In while loop of stageChunk 1\n");
		
		if (Xrecv(netStageSock, reply, sizeof(reply), 0) <= 0) {
			Xclose(netStageSock);
			die(-1, "Unable to communicate with the server\n");
		}
		char reply1[XIA_MAX_BUF] = {0};
		char * start, *end;
		int n;
		start = reply;
		while((end=strstr(start +6,"ready"))>0){
			n=end-start;
			printf("n=%d\n",n);
			strncpy(reply1,start,n);
			start=end;
			reply1[n]=0;
//say("*********************************In while loop of stageChunk 2\n");
			char oldDag[256];
			char newDag[256];
			sscanf(reply1, "%*s %s %s %ld", oldDag, newDag, &timeInt);
			updateStageArg();
//say("*********************************In while loop of stageChunk 3\n");
			say("cmd: %s\n", reply1);
			pthread_mutex_lock(&profileLock);
//say("*********************************In while loop of stageChunk 4\n");
			CIDToProfile[oldDag].dag = newDag;
			CIDToProfile[oldDag].state = READY;
			CIDToProfile[oldDag].stageFinishTimestemp = now_msec();
			managerTime << "OldDag: " << oldDag << " NewDag: " << newDag << " StageTime: " << CIDToProfile[oldDag].stageFinishTimestemp - CIDToProfile[oldDag].stageStartTimestemp << " ms." << endl;
			pthread_mutex_unlock(&profileLock);
		}
			//say("*********************************In while loop of stageChunk 2\n");
			char oldDag[256];
			char newDag[256];
			sscanf(start, "%*s %s %s %ld", oldDag, newDag, &timeInt);
			updateStageArg();
			//say("*********************************In while loop of stageChunk 3\n");
			say("cmd: %s\n", start);
			pthread_mutex_lock(&profileLock);
			//say("*********************************In while loop of stageChunk 4\n");
			CIDToProfile[oldDag].dag = newDag;
			CIDToProfile[oldDag].state = READY;
			CIDToProfile[oldDag].stageFinishTimestemp = now_msec();
			managerTime << "OldDag: " << oldDag << " NewDag: " << newDag << " StageTime: " << CIDToProfile[oldDag].stageFinishTimestemp - CIDToProfile[oldDag].stageStartTimestemp << " ms." << endl;
			pthread_mutex_unlock(&profileLock);
	}
	//pthread_mutex_unlock(&chunkMutex);
say("*********************************Exit while loop of stageChunk\n");
	pthread_exit(NULL);
}
// WORKING version
// TODO: scheduling
// control plane: figure out the right DAGs to stage, change the stage state from BLANK to PENDING to READY
void *stageData(void *)
{
    thread_c++;
    int thread_id = thread_c;
    cerr << "Thread id " << thread_id << ": " << "Is launched\n";
    cerr << "Current " << getAD2(0) << endl;
	//int chunkToRecv=0;
    char myAD[MAX_XID_SIZE];
    char myHID[MAX_XID_SIZE];
    char stageAD[MAX_XID_SIZE];
    char stageHID[MAX_XID_SIZE];

//netStageSock is used to communicate with stage server.
	
    getNewAD2(0, myAD);
	string lastSSID = getSSID();

    //Connect to the Stage Server
    int netStageSock = registerMulStageService(0, getStageServiceName2(0));
    //Rtt of wireless
    rttWifi = getRTT(getStageServiceName2(0));
    char rttCMD[100] = "";
    //Ask the Stage server to get the Rtt of Internet
    sprintf(rttCMD, "xping %s", getXftpName());
    if (Xsend(netStageSock, rttCMD, strlen(rttCMD), 0) < 0
            || Xrecv(netStageSock, rttCMD, sizeof(rttCMD), 0) < 0) {
        die(-1,"Unable to get the RTT.\n");
    }
    sscanf(rttCMD, "rtt %d", &rttInt);
    //Update stage window
    updateStageArg();
    say("++++++++++++++++++++++++++++++++++++The current netStageSock is %d\n", netStageSock);
    say("RTTWireless = %d RTTInternet = %d\n", rttWifi, rttInt);
    if (netStageSock == -1) {
        say("netStageOn is false!\n");
        netStageOn = false;
        pthread_exit(NULL);
    }
    pthread_t thread_stageData;
	int *lastSock = new int[2];
	lastSock[0] = netStageSock;
	//lastSock[1] = static_cast<int>(needStage.size());
	//lastSock[2] = chunkToRecv;
	pthread_create(&thread_stageData, NULL, stageChunk, (void*)lastSock);
    //pthread_mutex_unlock(&StageControl);
    // TODO: need to handle the case that new SID joins dynamically
    // TODO: handle no in-net staging service
    while (1) {
        say("*********************************Before while loop of stageData\n");
        pthread_mutex_lock(&StageControl);
		pthread_mutex_unlock(&fetchLock);
        say("*********************************In while loop of stageData\n");
        if(!isConnect()){
            long newStamp = now_msec();
            connetTime << lastSSID << " disconnect. Last: " << newStamp - timeStamp << "ms." << endl;
        }
        string currSSID = getSSID();
        if (lastSSID != currSSID) {
            connetTime << currSSID << "Connect." << endl;
            timeStamp = now_msec();
            say("Thread id: %d Network changed, create another thread to continue!\n");
            lastSSID = currSSID;
            pthread_t thread_stageDataNew;
            pthread_mutex_unlock(&StageControl);
			
			pthread_mutex_lock(&stopMutex);
			stopFlag = 0;
			pthread_mutex_unlock(&stopMutex);
			getNewAD2(0, myAD);
            pthread_create(&thread_stageDataNew, NULL, stageData, NULL);
            pthread_exit(NULL);
        }
		pthread_mutex_lock(&stopMutex);
		if(stopFlag == 1){
			pthread_mutex_unlock(&stopMutex);
			pthread_mutex_unlock(&StageControl);
			continue;
		}
		pthread_mutex_unlock(&stopMutex);
        vector<string> needStage;
        pthread_mutex_lock(&dagVecLock);
        //for (auto pair : allDAGs) {
            //int sock = pair.first;
		vector<string>& dags = allDAGs;//pair.second;
		//say("Handling the sock: %d\n", sock);
		pthread_mutex_lock(&profileLock);
		pthread_mutex_lock(&stageMutex);
		int beg = fetchIndex;
		//fetchIndex = -1;
		say("AlreadyStage: %d chunkToStage: %d, beg = %d\n",alreadyStage, chunkToStage,beg);
		if (alreadyStage >= chunkToStage || beg == -1){
			pthread_mutex_unlock(&stageMutex);
			pthread_mutex_unlock(&profileLock); 
			pthread_mutex_unlock(&dagVecLock);
			continue;
		}
		int needchunk = chunkToStage - alreadyStage;
		for (int i = beg, j = 0; j < needchunk && i < int(dags.size()); ++i) {
			//say("Before needStage: i = %d, beg = %d, dag = %s State: %d\n",i, beg, dags[i].c_str(),CIDToProfile[dags[i]].state);
			if (CIDToProfile[dags[i]].state == BLANK || CIDToProfile[dags[i]].state == PREFETCH) {
				//say("needStage: i = %d, beg = %d, dag = %s\n",i, beg, dags[i].c_str());
				CIDToProfile[dags[i]].state = PENDING;
				needStage.push_back(dags[i]);
				j++;
			}
		}
		alreadyStage += needStage.size();
		pthread_mutex_unlock(&stageMutex);
		pthread_mutex_unlock(&profileLock);
		say("Size of NeedStage: %d", needStage.size());
		if (needStage.size() == 0) {
			pthread_mutex_unlock(&dagVecLock);
			continue;
		}
		
		char cmd[XIA_MAX_BUF] = {0};
		int cnt=0;
	    sprintf(cmd, "stage");
		for (auto dag : needStage) {
			CIDToProfile[dag].stageStartTimestemp = now_msec();
			sprintf(cmd, "%s %s",cmd, dag.c_str());
			cnt++;
			if(cnt == 2){
				cnt = 0;
				sendStreamCmd(netStageSock, cmd);
				memset(cmd, 0, sizeof(cmd));
				sprintf(cmd, "stage");
			}
		}
		if(strlen(cmd) > 8)
			sendStreamCmd(netStageSock, cmd);
		
		//chunkToRecv += needStage.size();
		pthread_mutex_unlock(&dagVecLock);
    }
        
}
void *preStageData(void *)
{
    thread_c++;
    int thread_id = thread_c;
	int chunkToRecv=0;
	char AD1[MAX_XID_SIZE];
	char AD2[MAX_XID_SIZE];
    char myAD[MAX_XID_SIZE];
    char myHID[MAX_XID_SIZE];
    char stageAD[MAX_XID_SIZE];
    char stageHID[MAX_XID_SIZE];
	int preStageChunk=0;
	string	lastSSID = getSSID();
	strcpy(AD1, "AD:d760f2fd8572b5113d7a7c3b94be28838e97666f");
	strcpy(AD2, "AD:8a912e21cf0f3fb78eb7e6e7e99f8a70c92f8de8");
    getNewAD2(0, myAD);
	strcpy(myAD, getAD2(0).c_str());
	cerr << "preStageData Thread id " << thread_id << ": " << "Is launched\n";
    cerr << "preStageData Current " << getAD2(0) << endl;
    //Connect to the Stage Server
    string serviceAD = "";
	if(strcmp(myAD, AD1) == 0)serviceAD = AD2;
	if(strcmp(myAD, AD2) == 0)serviceAD = AD1;
	say("serviceAD = \n%s\n", serviceAD.c_str());
	int netStageSock = registerMulStageService(0, getStageServiceName3(serviceAD));
    say("+++++++++++++++++++++++++++In preStageData, the current netStageSock is %d\n", netStageSock);
    if (netStageSock == -1) {
        say("netStageOn is false!\n");
        netStageOn = false;
        pthread_exit(NULL);
    }

    
	while(1){
		say("*********************************---------------before preStageData lock\n");	
		pthread_mutex_lock(&preStageControl);
        say("*********************************---------------after  preStageData lock\n");
        string currSSID = getSSID();
        if (lastSSID != currSSID) {
			pthread_mutex_lock(&stopMutex);
			stopFlag = 0;
			pthread_mutex_unlock(&stopMutex);
            pthread_exit(NULL);
        }
        //if(strcmp(myAD, getAD2(0).c_str()) != 0){
		//	pthread_exit(NULL);
		//}		
		
        vector<string> needStage;
        pthread_mutex_lock(&dagVecLock);
		vector<string>& dags = allDAGs;//pair.second;
	
		pthread_mutex_lock(&profileLock);
		pthread_mutex_lock(&stageMutex);
		int beg = fetchIndex;
		//fetchIndex = -1;
		say("AlreadyStage: %d chunkToStage: %d  in preStageData, beg= %d\n",alreadyStage, chunkToStage, beg);
		if (alreadyStage+preStageChunk >= chunkToStage + 1 || beg == -1){
			pthread_mutex_unlock(&stageMutex);
			pthread_mutex_unlock(&profileLock); 
			pthread_mutex_unlock(&dagVecLock);
			continue;
		}
		int needchunk = chunkToStage + 1 - alreadyStage - preStageChunk ;
		
		for (int i = beg, j = 0; j < needchunk && i < int(dags.size()); ++i) {
			//say("Before needStage: i = %d, beg = %d, dag = %s State: %d\n",i, beg, dags[i].c_str(),CIDToProfile[dags[i]].state);
			if (CIDToProfile[dags[i]].state == BLANK) {
				//say("needStage: i = %d, beg = %d, dag = %s\n",i, beg, dags[i].c_str());
				CIDToProfile[dags[i]].state = PREFETCH;
				needStage.push_back(dags[i]);
				j++;
			}
		}

		preStageChunk += needStage.size();
		pthread_mutex_unlock(&stageMutex);
		pthread_mutex_unlock(&profileLock);
		say("Size of NeedStage: %d", needStage.size());
		if (needStage.size() == 0) {
			pthread_mutex_unlock(&dagVecLock);
			continue;
		}
		char cmd[XIA_MAX_BUF] = {0};
		int cnt=0;
	    sprintf(cmd, "prestage");
		for (auto dag : needStage) {
			CIDToProfile[dag].stageStartTimestemp = now_msec();
			sprintf(cmd, "%s %s",cmd, dag.c_str());
			cnt++;
			if(cnt == 2){
				cnt = 0;
				sendStreamCmd(netStageSock, cmd);
				memset(cmd, 0, sizeof(cmd));
				sprintf(cmd, "prestage");
			}
		}
		if(strlen(cmd) > 8)
			sendStreamCmd(netStageSock, cmd);
		
		pthread_mutex_unlock(&dagVecLock);
	}
    pthread_exit(NULL);
}

void * time_control(void *){
switch (HANDOFFPOLICY) 
	{
		case 0:
			pthread_mutex_lock(&encounterTime);	
			while(1){
				for(int i = 0; i < netsize; ++i){
					usleep( (CONNECT_TIME - OVERLAP_TIME - SOFTSTAGE_TIME) * 1000);
					pthread_mutex_unlock(&encounterTime);
					usleep( SOFTSTAGE_TIME * 1000 );
					pthread_mutex_unlock(&encounterTime);
				}
			}
			break;
		case 1:
			pthread_mutex_lock(&encounterTime);	
			while(1){
				for(int i = 0; i < netsize; ++i){
					usleep( OVERLAP_TIME * 1000);
					pthread_cond_signal(&handoffCond);
					usleep( (CONNECT_TIME - 2*OVERLAP_TIME) * 1000);
					pthread_mutex_unlock(&encounterTime);
				}
			}
			break;
		case 2:
			pthread_mutex_lock(&encounterTime);	
			while(1){
				for(int i = 0; i < netsize; ++i){
					usleep( (OVERLAP_TIME - SOFTSTAGE_TIME) * 1000 );
					pthread_cond_signal(&handoffCond);
					usleep( (CONNECT_TIME - 2 * OVERLAP_TIME) * 1000);
					pthread_mutex_unlock(&encounterTime);
					usleep( SOFTSTAGE_TIME * 1000 );
					pthread_mutex_unlock(&encounterTime);
				}
			}
			break;
		default:
			break;
	}
}
void *handoffPolicy(void *){
	switch (HANDOFFPOLICY) 
	{
//stronger signal strength
		case 0:{
			//say("handoffPolicy: 0\n");
			string result = execSystem("service network-manager stop");
			printf("close_network_manager %s\n", result.c_str());
			Disconnect1(0);
			Disconnect1(1);
			pthread_t thread_time;
			pthread_create(&thread_time, NULL, time_control, NULL);
			while (1) {
				for(int j = 0; j < 2; ++j){
					for(int i = 0; i < netsize; ++i){	
						Connect1(0, j, FREQ[j]);
						pthread_mutex_unlock(&StageControl);
						pthread_mutex_unlock(&preStageControl);						
							
						pthread_mutex_lock(&encounterTime);						
						pthread_mutex_lock(&stopMutex);
						stopFlag = 1;
						pthread_mutex_unlock(&stopMutex);
						pthread_t Thread_preStageData;
						pthread_mutex_unlock(&preStageControl);
						pthread_create(&Thread_preStageData, NULL, preStageData, NULL);							
						
						pthread_mutex_lock(&encounterTime);	
						Disconnect1(0);
					}
				}
			}
		}
			break;
//finish fetching current chunk
		case 1:
			{
			string result = execSystem("service network-manager stop");
			printf("close_network_manager %s\n", result.c_str());
			Disconnect1(0);
			Disconnect1(1);
			pthread_t thread_time;
			pthread_create(&thread_time, NULL, time_control, NULL);
			
			while (1) {
				for(int j = 0; j < 2; ++j){
					for(int i = 0; i < netsize; ++i){	
						Connect1(0, j, FREQ[j]);
						pthread_mutex_unlock(&StageControl);
						pthread_mutex_unlock(&preStageControl);
					
						pthread_mutex_lock(&handoffModeMutex);
						if(handoffMode == 2)handoffMode = 0;
						pthread_mutex_unlock(&handoffModeMutex);
						pthread_cond_signal(&handoffCond);

						pthread_mutex_lock(&encounterTime);					
						pthread_mutex_lock(&stopMutex);
						stopFlag = 1;
						pthread_mutex_unlock(&stopMutex);
						pthread_t Thread_preStageData;
						pthread_mutex_unlock(&preStageControl);
						pthread_create(&Thread_preStageData, NULL, preStageData, NULL);
						
						pthread_mutex_lock(&handoffModeMutex);
						while(handoffMode == 1) pthread_cond_wait(&handoffCond, &handoffModeMutex);//?????/?while
						handoffMode = 2;
						pthread_mutex_unlock(&handoffModeMutex);
						Disconnect1(0);
						
					}
				}
			}break;
		}
//when signal strength is better, finish fetching current chunk
		case 2:
		{
			string result = execSystem("service network-manager stop");
			printf("close_network_manager %s\n", result.c_str());
			Disconnect1(0);
			Disconnect1(1);
			pthread_t thread_time;
			pthread_create(&thread_time, NULL, time_control, NULL);
			
			while (1) {
				for(int j = 0; j < 2; ++j){
					for(int i = 0; i < netsize; ++i){	
						Connect1(0, j, FREQ[j]);
						pthread_mutex_unlock(&StageControl);
						pthread_mutex_unlock(&preStageControl);
						
						pthread_mutex_lock(&handoffModeMutex);
						if(handoffMode == 2)handoffMode = 0;
						pthread_mutex_unlock(&handoffModeMutex);
						pthread_cond_signal(&handoffCond);
	
						pthread_mutex_lock(&encounterTime);	
						pthread_mutex_lock(&stopMutex);
						stopFlag = 1;
						pthread_mutex_unlock(&stopMutex);
						pthread_t Thread_preStageData;
						pthread_mutex_unlock(&preStageControl);
						pthread_create(&Thread_preStageData, NULL, preStageData, NULL);
						
						pthread_mutex_lock(&encounterTime);	
						pthread_mutex_lock(&handoffModeMutex);
						while(handoffMode == 1) pthread_cond_wait(&handoffCond, &handoffModeMutex);//?????/?while
						handoffMode = 2;
						pthread_mutex_unlock(&handoffModeMutex);	
						Disconnect1(0);
						
					}
				}
			}
		}break;
		default:
			break;
    }
	pthread_exit(NULL);
}

int main(int argc, char **argv)
{
//say("at start\n");
	getConfig(argc, argv);
//pthread_mutex_lock(&StageControl);
//say("before getSSID\n");
    //lastSSID = getSSID();
//say("after getSSID\n");
    //currSSID = lastSSID;
    timeStamp = now_msec();
    //connetTime << currSSID << "Connect." << endl;
    int stageSock = registerUnixStreamReceiver(UNIXMANAGERSOCK);
if(stageSock <= 0 )die(-1, "registerUnixStreamReceiver failed");
//say("after registerUnixStreamReceiver:%d\n",stageSock);
	pthread_t thread_handoffPolicy;
    pthread_create(&thread_handoffPolicy, NULL, handoffPolicy, NULL);
//say("after thread_handoffPolicy\n");
    pthread_t thread_stageData;
    pthread_create(&thread_stageData, NULL, stageData, NULL);
	
    UnixBlockListener((void*)&stageSock, clientCmd);
    return 0;
}

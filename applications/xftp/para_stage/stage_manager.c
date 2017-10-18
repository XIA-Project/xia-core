#include "stage_utils.h"

using namespace std;
string lastSSID, currSSID;
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

long Timeline=-1, StartTime;
int stageCount = 0;
// TODO: this is not easy to manage in a centralized manner because we can have multiple threads for staging data due to mobility and we should have a pair of mutex and cond for each thread
pthread_mutex_t profileLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dagVecLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t stageMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t StageControl = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fetchLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t chunkMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t responseMutex = PTHREAD_MUTEX_INITIALIZER;

long timeStamp;
ofstream connetTime("connetTime.log");
ofstream windowToStage("windowToStage.log");
ofstream managerTime("managerTime.log");
ofstream stageTime("stageTime.log");
int STAGEPOLICY = 0;
int CONNECT_TIME = 32000;
void getConfig(int argc, char** argv)
{
    int c;
    opterr = 0;
    while ((c = getopt(argc, argv, "c:s:")) != -1) {
        switch (c) {
			case 'c':
				CONNECT_TIME = atoi(optarg);
				break;
			case 's':
				STAGEPOLICY = atoi(optarg);
				break;
            default:
                    break;
    	}
    }
}
//Update the stage window
void updateStageArg() {
	if(STAGEPOLICY!=0){
		chunkToStage = STAGEPOLICY;
	}
    else if (rttWifi != -1 && timeWifi && timeInt) {
        int left = timeWifi ;//rttWifi是stage_manager和stage_server; timeWifi是client下载一个chunk的时间
        int right = timeInt + rttWifi ;//rttInt是stage_server到content_server; timeInt是stage一个chunk要花的时间
        chunkToStage = static_cast<double>(right) / left + 1;
    }
    else {
        chunkToStage = 3;
    }

    windowToStage << "fetchRTT:	" << rttWifi
                  << "	fetchlatency:	" << timeWifi
                  << "	stagelatency:	" << timeInt 
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
//say("Pending-----In delegationHandler while.\nThe command is %s\n", cmd);
            pthread_mutex_lock(&profileLock);
            if (CIDToProfile[cmd].state == READY) {
                //CIDToProfile[cmd].state = IGNORE;
				alreadyStage--;

				
                break;
                say("DAG: %s change into IGNORE!\n", cmd);
            }
			if(CIDToProfile[cmd].state == BLANK){
	    	    CIDToProfile[cmd].state = READY;
				//pthread_mutex_unlock(&fetchLock);
				break;
            //    CIDToProfile[cmd].dag = cmd;
            }
            pthread_mutex_unlock(&profileLock);
        }
        tmp = CIDToProfile[cmd].dag;
        //CIDToProfile.erase(cmd);
        pthread_mutex_unlock(&profileLock);
////say("In delegationHandler. stageMutex\nThe command is %s\n", cmd);
        pthread_mutex_lock(&stageMutex);
        fetchIndex = dis;
        
        pthread_mutex_unlock(&stageMutex);
		pthread_mutex_unlock(&StageControl);
    }
//say("In delegationHandler. send\nThe command is %s\n", cmd);
    if (send(sock, tmp.c_str(), tmp.size(), 0) < 0) {
        warn("socket error while sending data, closing connetTime\n");
        return -1;
    }
    say("End of delegationHandler\n");
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
			die(-1, "unix socket error while waiting for data\n");
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
            sscanf(cmd, "fetch %s" ,dag);
            if (delegationHandler(sock, dag) < 0) {
                break;
            }
            
            //pthread_mutex_unlock(&StageControl);
        }
        else if (strncmp(cmd, "time", 4) == 0) {
            say("Get Time! cmd: %s\n", cmd);
			long fetchTime;
            sscanf(cmd, "time %ld", &fetchTime);
			if (send(sock, "Get", sizeof("Get"), 0) < 0) {
       			warn("socket error while sending data, closing connetTime\n");
    		}
            if(fetchTime <= CONNECT_TIME){
				timeWifi = fetchTime;
				updateStageArg();
			}
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
say("*********************************Before while loop of stageChunk\n");
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
say("*********************************In while loop of stageChunk 1\n");
		
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
			//printf("n=%d\n",n);
			strncpy(reply1,start,n);
			start=end;
			reply1[n]=0;
			say("*********************************In while loop of stageChunk 2\n");
			char oldDag[256];
			char newDag[256];
			sscanf(reply1, "%*s %s %s %ld", oldDag, newDag, &timeInt);
			updateStageArg();
			say("*********************************In while loop of stageChunk 3\n");
			say("cmd: %s\n", reply1);
			pthread_mutex_lock(&profileLock);
			say("*********************************In while loop of stageChunk 4\n");
			CIDToProfile[oldDag].dag = newDag;
			CIDToProfile[oldDag].state = READY;
			CIDToProfile[oldDag].stageFinishTimestemp = now_msec();
			//managerTime << "OldDag: " << oldDag << " NewDag: " << newDag << " StageTime: " << CIDToProfile[oldDag].stageFinishTimestemp - CIDToProfile[oldDag].stageStartTimestemp << " ms." << endl;
			pthread_mutex_unlock(&profileLock);
		}
			say("*********************************In while loop of stageChunk 2\n");
			char oldDag[256];
			char newDag[256];
			sscanf(start, "%*s %s %s %ld", oldDag, newDag, &timeInt);
			updateStageArg();
			say("*********************************In while loop of stageChunk 3\n");
			say("cmd: %s\n", start);
			pthread_mutex_lock(&profileLock);
			say("*********************************In while loop of stageChunk 4\n");
			CIDToProfile[oldDag].dag = newDag;
			CIDToProfile[oldDag].state = READY;
			CIDToProfile[oldDag].stageFinishTimestemp = now_msec();
			
			Timeline += timeInt;
			stageCount++;
			//stageTime << Timeline << "	" << stageCount << endl;
		

			//managerTime << "OldDag: " << oldDag << " NewDag: " << newDag << " StageTime: " << CIDToProfile[oldDag].stageFinishTimestemp - CIDToProfile[oldDag].stageStartTimestemp << " ms." << endl;
			pthread_mutex_unlock(&profileLock);
			//cnt++;
			//pthread_mutex_unlock(&responseMutex);

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
long startStamp = now_msec();
    thread_c++;
    int thread_id = thread_c;
    //cerr << "Thread id " << thread_id << ": " << "Is launched\n";
    //cerr << "Current " << getAD() << endl;
	int chunkToRecv=0;
    char myAD[MAX_XID_SIZE];
    char myHID[MAX_XID_SIZE];
    char stageAD[MAX_XID_SIZE];
    char stageHID[MAX_XID_SIZE];

//netStageSock is used to communicate with stage server.
    getNewAD(myAD);
	string lastSSID = getSSID();
    //Connect to the Stage Server
long endStamp = now_msec();
say("Total using time1 = %ld ms\n", endStamp - startStamp);
    int netStageSock = registerStageService(getStageServiceName(), myAD, myHID, stageAD, stageHID);
    //Rtt of wireless
endStamp = now_msec();
say("Total using time2 = %ld ms\n", endStamp - startStamp);

    //rttWifi = getRTT(getStageServiceName());
    char rttCMD[100] = "";
    //Ask the Stage server to get the Rtt of Internet
/*
    sprintf(rttCMD, "xping %s", getXftpName());
    if (Xsend(netStageSock, rttCMD, strlen(rttCMD), 0) < 0
            || Xrecv(netStageSock, rttCMD, sizeof(rttCMD), 0) < 0) {
        die(-1,"Unable to get the RTT.\n");
    }
    sscanf(rttCMD, "rtt %d", &rttInt);
*/
endStamp = now_msec();
say("Total using time3 = %ld ms\n", endStamp - startStamp);
    //Update stage window
    //updateStageArg();
    say("++++++++++++++++++++++++++++++++++++The current netStageSock is %d\n", netStageSock);
   // say("RTTWireless = %d RTTInternet = %d\n", rttWifi, rttInt);
    if (netStageSock == -1) {
        say("netStageOn is false!\n");
        netStageOn = false;
        pthread_exit(NULL);
    }
endStamp = now_msec();
say("Total using time = %ld ms\n", endStamp - startStamp);
    pthread_t thread_stageData;
	int *lastSock = new int[2];
	lastSock[0] = netStageSock;
	//lastSock[1] = static_cast<int>(needStage.size());
	lastSock[2] = chunkToRecv;
	pthread_create(&thread_stageData, NULL, stageChunk, (void*)lastSock);
    //pthread_mutex_unlock(&StageControl);
    // TODO: need to handle the case that new SID joins dynamically
    // TODO: handle no in-net staging service
    while (1) {
        say("*********************************Before while loop of stageData\n");
        pthread_mutex_lock(&StageControl);
	//pthread_mutex_unlock(&fetchLock);
        say("*********************************In while loop of stageData\n");
        if(!isConnect()){
            long newStamp = now_msec();
            //connetTime << lastSSID << " disconnect. Last: " << newStamp - timeStamp << "ms." << endl;
        }
//long beforeget = now_msec();
        string currSSID = getSSID();
//long afterget = now_msec();
//printf("Total get SSID using time = %ld ms\n",  afterget - beforeget );

say("current SSID = %s\n\n\n", currSSID.c_str());
        if (lastSSID != currSSID) {
            //connetTime << currSSID << "Connect." << endl;
            timeStamp = now_msec();
            say("Thread id: %d Network changed, create another thread to continue!\n");
            //lastSSID = currSSID;
            //pthread_t thread_stageDataNew;
            pthread_mutex_unlock(&StageControl);
            //pthread_create(&thread_stageDataNew, NULL, stageData, NULL);
            pthread_exit(NULL);
        }
        vector<string> needStage;
        pthread_mutex_lock(&dagVecLock);
        //for (auto pair : allDAGs) {
            //int sock = pair.first;
            vector<string>& dags = allDAGs;//pair.second;
            //say("Handling the sock: %d\n", sock);
            pthread_mutex_lock(&profileLock);
            pthread_mutex_lock(&stageMutex);
            int beg = fetchIndex;
            fetchIndex = -1;
            say("AlreadyStage: %d chunkToStage: %d\n",alreadyStage, chunkToStage);
            if (alreadyStage >= chunkToStage || beg == -1){
                pthread_mutex_unlock(&stageMutex);
                pthread_mutex_unlock(&profileLock); 
				pthread_mutex_unlock(&dagVecLock);
                continue;
            }
			int needchunk = chunkToStage - alreadyStage;
            for (int i = beg, j = 0; j < needchunk && i < int(dags.size()); ++i) {
                say("Before needStage: i = %d, beg = %d, dag = %s State: %d\n",i, beg, dags[i].c_str(),CIDToProfile[dags[i]].state);
                if (CIDToProfile[dags[i]].state == BLANK) {
                    say("needStage: i = %d, beg = %d, dag = %s\n",i, beg, dags[i].c_str());
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
            
            //char reply[XIA_MAX_BUF] = {0};
/*			pthread_mutex_lock(&responseMutex);
            sprintf(cmd, "stage cont ");
            for (auto dag : needStage) {
                //say("needStage: %s\n", dag.c_str());
                CIDToProfile[dag].stageStartTimestemp = now_msec();
                sprintf(cmd, "stageCont %s\0", dag.c_str());
				sendStreamCmd(netStageSock, cmd);
            }
	    sprintf(cmd, "stageEnd");
*/
	    /*
	    char cmd[XIA_MAX_BUF] = {0};
	    sprintf(cmd, "stage");
            for (auto dag : needStage) {
                //say("needStage: %s\n", dag.c_str());
                CIDToProfile[dag].stageStartTimestemp = now_msec();
                sprintf(cmd, "%s %s",cmd, dag.c_str());
            }
	    sendStreamCmd(netStageSock, cmd);
	    */
//printf("Stage %d chunks!!!!!!!!!!!\n\n", needStage.size());
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
		chunkToRecv += needStage.size();

		if(Timeline == -1){
			StartTime = now_msec();
			Timeline = 0;
			//stageTime << "0	0" << endl;
		}else{
			Timeline = now_msec() - StartTime;
			//stageTime << Timeline << "	" << stageCount << endl;
		}
		//pthread_mutex_lock(&responseMutex);
            //sendStreamCmd(netStageSock, cmd);
		//pthread_mutex_unlock(&responseMutex);
			
	    //if(chunkToStage - alreadyStage > 0)pthread_mutex_unlock(&StageControl);
            /*for (int i = 0; i < static_cast<int>(needStage.size()); ++i) {
                sayHello(netStageSock, "READY TO RECEIVE!\n");
                memset(reply,0,sizeof(reply));
                if (Xrecv(netStageSock, reply, sizeof(reply), 0) < 0) {
                    Xclose(netStageSock);
                    die(-1, "Unable to communicate with the server\n");
                }
                char oldDag[256];
                char newDag[256];
                sscanf(reply, "%*s %s %s %ld", oldDag, newDag, &timeInt);
                updateStageArg();
                say("cmd: %s\n", reply);
                pthread_mutex_lock(&profileLock);
                CIDToProfile[oldDag].dag = newDag;
                CIDToProfile[oldDag].state = READY;
                CIDToProfile[oldDag].stageFinishTimestemp = now_msec();
                managerTime << "OldDag: " << oldDag << " NewDag: " << newDag << " StageTime: " << CIDToProfile[oldDag].stageFinishTimestemp - CIDToProfile[oldDag].stageStartTimestemp << " ms." << endl;
                pthread_mutex_unlock(&profileLock);
            }
			*/
		pthread_mutex_unlock(&dagVecLock);
        }
        
    //}
    //pthread_exit(NULL);
}
void *watchNetwork(void *){
	char myAD[MAX_XID_SIZE];
	//string lastSSID = "";
	while(true){
			getNewAD(myAD);
		//string currSSID = getSSID();
		//if(lastSSID != currSSID){
			//lastSSID = currSSID;
			//printf("SSID changed! %s unlock! \n\n", currSSID.c_str() );
		
			pthread_t thread_stageDataNew;
            pthread_create(&thread_stageDataNew, NULL, stageData, NULL);
			rttWifi = getRTT(getStageServiceName());
			updateStageArg();
			
			//usleep(100  *1000);
		//}
	}
}
int main(int argc, char **argv)
{
//pthread_mutex_lock(&StageControl);
//say("before getSSID");
	getConfig(argc, argv);
    lastSSID = getSSID();
//say("after getSSID");
    //currSSID = lastSSID;
    timeStamp = now_msec();
    //connetTime << currSSID << "Connect." << endl;
    int stageSock = registerUnixStreamReceiver(UNIXMANAGERSOCK);
//say("after registerUnixStreamReceiver");
    //pthread_t thread_stageData;
    //pthread_create(&thread_stageData, NULL, stageData, NULL);
	
	pthread_t thread_watchNetwork;
    pthread_create(&thread_watchNetwork, NULL, watchNetwork, NULL);
//say("after stageData");
    UnixBlockListener((void*)&stageSock, clientCmd);
//say("after clientCmd");
    return 0;
}

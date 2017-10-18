#include "stage_utils.h"

#define SCAN_INTERVAL 10
#define MAX_SIZE 1024 * 10

#define INTERFACE1 "wlp1s0 "
#define INTERFACE2 "wlx60a44ceca928 "

int CONNECT_TIME = 8 * 1000;
int DISCONNECT_TIME = 32 * 1000;
int netsize = 1;
int FREQ;
int TRACE = 0;
pthread_mutex_t encounterTime = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t disconnectTime = PTHREAD_MUTEX_INITIALIZER;
double ct1[9] = {7, 2.7, 6.5, 3.5, 2.1, 2, 16.2, 3.2, 5.7};
double ct2[9] = {13.5, 1.7, 60, 2.6, 4.4,1.4, 1.0, 11.0, 9.3};
#define GET_SSID_LIST "iwlist wlp6s0 scanning | grep -E '(\\\"[a-zA-Z0-9 _-.]*\\\")|(Signal level=-?[0-9]* dBm)' -o"
#define CLOSE_NETWORK_MANAGER "service network-manager stop"
void getConfig(int argc, char** argv)
{
	int c;

	opterr = 0;

	while ((c = getopt(argc, argv, "c:d:n:f:t:")) != -1) {
		switch (c) {
			case 'c':
				CONNECT_TIME = atoi(optarg);
				break;
			case 'd':
				DISCONNECT_TIME = atoi(optarg);
				break;
			case 'n':
				netsize = atoi(optarg);
				break;
			case 'f':
				FREQ = atoi(optarg);
				break;
			case 't':
				TRACE = atoi(optarg);
				break;
			default:
				break;
		}
	}
}
int cast_sig(char * p_sig){
//say("in cast_sig\n");
	int sig=0;
	int flag = 1;
	p_sig += 13;
	if(*p_sig == '-'){
		flag = -1;
		p_sig++;
	}
	for(int i = 0; p_sig[i] != 0 ;++i){
		if(p_sig[i] == ' ')
			p_sig[i] = 0;	
	}
	sig = atoi(p_sig);
//say("sig = %d\n",sig * flag);
	return sig * flag;
}

string get_SSID(int interface){
say("int get_SSID\n");
	string result = "";
	string cmd = "iwconfig ";
	if(interface == 1){
		cmd += INTERFACE1;
	}
	else if(interface == 2){
		cmd += INTERFACE2;
	}
	else
		return result;
	cmd += "| grep '\\\"[a-zA-Z0-9 _-.]*\\\"' -o";
say("cmd = %s\n", cmd.c_str());
	string ssid = execSystem(cmd);
say("ssid = %s\n", ssid.c_str());
	return ssid;
}

int disconnect_SSID(int interface){
say("in disconnect_SSID\n");
	string result;
	string cmd="";

	cmd += "iw ";
	if(interface == 1){
		cmd += INTERFACE1;
	}
	else if(interface == 2){
		cmd += INTERFACE2;
	}
	else
		return -1;
	cmd += "disconnect";
say("cmd = %s\n", cmd.c_str());
	result = execSystem(cmd);
	say("disconnect to SSID: %s\n", result.c_str());
	return 0;
}

int connect_SSID(int interface, char * ssid,int f){
say("in connect_SSID\n");
	string result;
	string cmd1,cmd2;

	cmd1 = "iw ";
	if(interface == 1){
		cmd1 += INTERFACE1;
	}
	else if(interface == 2){
		cmd1 += INTERFACE2;
	}
	else
		return -1;
	cmd1 += "connect ";
	cmd1 += ssid;
	cmd1 += " ";
	char freq_string[16];
	if(f == 1){//"XIA_2.4_1"
		sprintf(freq_string, "%d", 2447);
	}
	else if(f == 2){//"XIA_2.4_1"
		sprintf(freq_string, "%d", 2442);
	}
	else if(f == 3){//"XIA_5_1"
		sprintf(freq_string, "%d", 5745);
	}
	else if(f == 4){//"XIA_5_1"
		sprintf(freq_string, "%d", 5765);
	}
	cmd1 += freq_string;
	
say("cmd1 = %s\n", cmd1.c_str());
	result = execSystem(cmd1);
	say("connect to SSID: %s\n", result.c_str());
cmd2 = "iw dev ";
	if(interface == 1){
		cmd2 += INTERFACE1;
	}
	else if(interface == 2){
		cmd2 += INTERFACE2;
	}
	else
		return -1;
	cmd2 += "link";
say("cmd2 = %s\n", cmd2.c_str());
	int looptime=0;
	while(1){
	++looptime;
	result = execSystem(cmd2);
        say("check SSID: %s\n", result.c_str());
	    if(result != "Not connected."){
		say("connected!\n");
		break;
	    }
	usleep(10 * 1000);
	
	if (looptime>=5){
		connect_SSID(interface,ssid, f);
	}
	}
	result = execSystem("date '+%c%N'");
        say("connect time: %s\n", result.c_str());
	return 0;
}
int disconnect(int interface){
	long begin_time, end_time, total_time;
	int rtn;
	pthread_mutex_lock(&disconnectTime);
	begin_time = now_msec();
	say("-------------disconnect begin at %ld \n", begin_time);
	rtn = disconnect_SSID(interface);
	//usleep(DISCONNECT_TIME * 1000);
	end_time = now_msec();
	say("-------------disconnect end at %ld \n", end_time);
	total_time = end_time - begin_time;
	say("-------------disconnect using time = %ld \n", total_time);
}
int connect(int interface, char * ssid, int f){
	int rtn;
	long begin_time, end_time, total_time;
	pthread_mutex_lock(&encounterTime);
	begin_time = now_msec();
	say("-------------connect begin at %ld \n", begin_time);
	rtn = connect_SSID(interface, ssid, f);
	//usleep(CONNECT_TIME * 1000);
	end_time = now_msec();
	say("-------------connect end at %ld \n", end_time);
	total_time = end_time - begin_time;
	say("-------------connect using time = %ld \n", total_time);
}
void * time_control(void *){
	if(TRACE == 0){
		while(1){
		for(int i = 0; i < netsize; ++i){
			pthread_mutex_unlock(&encounterTime);
			usleep(CONNECT_TIME * 1000 / netsize);
			pthread_mutex_unlock(&disconnectTime);
		}
		usleep(DISCONNECT_TIME * 1000);
		}
	}else if (TRACE == 1){
		int cnt=0;
		while(cnt<9){
		for(int i = 0; i < netsize; ++i){
			pthread_mutex_unlock(&encounterTime);
			usleep(ct1[cnt++] *1000 * 1000 / netsize);
			pthread_mutex_unlock(&disconnectTime);
		}
		usleep(ct1[cnt++] * 1000* 1000);
		}
	}else if (TRACE == 2){
		int cnt=0;
		while(cnt<9){
		for(int i = 0; i < netsize; ++i){
			pthread_mutex_unlock(&encounterTime);
			usleep(ct2[cnt++] *1000 * 1000 / netsize);
			pthread_mutex_unlock(&disconnectTime);
		}
		usleep(ct2[cnt++] * 1000*1000);
		}
	}
}
int main(int argc, char **argv){
	getConfig(argc, argv);
	string result;	
	long begin_time, end_time, total_time;
	
	result = execSystem(CLOSE_NETWORK_MANAGER);
	say("close_network_manager %s\n", result.c_str());

	/*disconnect_SSID(1);
	say("connect time >> ");
	scanf("%d",&CONNECT_TIME);
	say("disconnect time >> ");
	scanf("%d",&DISCONNECT_TIME);
	say("Network size >> ");
	scanf("%d",&netsize);
	CONNECT_TIME *= 1000;
	DISCONNECT_TIME *= 1000;*/
pthread_mutex_lock(&encounterTime);
pthread_mutex_lock(&disconnectTime);
	pthread_t thread_time;
	
    	pthread_create(&thread_time, NULL, time_control, NULL);
	while (1) {			
		for(int i = 0; i < netsize; ++i){
			if(FREQ == 2 || FREQ == 3)
				connect(1, "XIA_2.4_1", 1);	
			else if(FREQ == 5)
				connect(1, "XIA_5_1", 3);
			disconnect(1);
		}
		for(int i = 0; i < netsize; ++i){
			if(FREQ == 2)
				connect(1, "XIA_2.4_2", 2);	
			else if(FREQ == 5 || FREQ == 3)
				connect(1, "XIA_5_2", 4);
			disconnect(1);
		}
	}
	return 0;
}

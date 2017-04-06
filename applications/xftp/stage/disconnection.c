#include "stage_utils.h"

#define SCAN_INTERVAL 10
#define MAX_SIZE 1024 * 10

#define INTERFACE1 "wlp1s0 "
#define INTERFACE2 "wlx60a44ceca928 "

int CONNECT_TIME = 8 * 1000;
int DISCONNECT_TIME = 32 * 1000;

#define GET_SSID_LIST "iwlist wlp6s0 scanning | grep -E '(\\\"[a-zA-Z0-9 _-.]*\\\")|(Signal level=-?[0-9]* dBm)' -o"
#define CLOSE_NETWORK_MANAGER "service network-manager stop"

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

int connect_SSID(int interface, char * ssid){
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
	usleep(10);
	/*
	if (looptime>10){
		connect_SSID(interface,ssid);
		break;
	}*/
	}
	return 0;
}
int disconnect(int interface){
	long begin_time, end_time, total_time;
	int rtn;
	begin_time = now_msec();
	printf("-------------disconnect begin at %ld \n", begin_time);
	rtn = disconnect_SSID(interface);
	usleep(DISCONNECT_TIME * 1000);
	end_time = now_msec();
	say("-------------disconnect end at %ld \n", end_time);
	total_time = end_time - begin_time;
	printf("-------------disconnect total time = %ld \n", total_time);
}
int connect(int interface, char * ssid){
	int rtn;
	long begin_time, end_time, total_time;
	begin_time = now_msec();
	printf("-------------connect begin at %ld \n", begin_time);
	rtn = connect_SSID(interface, ssid);
	usleep(CONNECT_TIME * 1000);
	end_time = now_msec();
	say("-------------connect end at %ld \n", end_time);
	total_time = end_time - begin_time;
	printf("-------------connect total time = %ld \n", total_time);
}
int main(){
	string result;	
	long begin_time, end_time, total_time;
	int netsize;
	result = execSystem(CLOSE_NETWORK_MANAGER);
	printf("close_network_manager %s\n", result.c_str());

	disconnect_SSID(1);
	printf("connect time >> ");
	scanf("%d",&CONNECT_TIME);
	printf("disconnect time >> ");
	scanf("%d",&DISCONNECT_TIME);
	printf("Network size >> ");
	scanf("%d",&netsize);
	CONNECT_TIME *= 1000;
	DISCONNECT_TIME *= 1000;
	while (1) {			
		for(int i = 0; i < netsize; ++i){
			connect(1, "XIA_Tenda_2");
			disconnect(1);
		}
		for(int i = 0; i < netsize; ++i){
			connect(1, "XIA-TP-LINK_5G");
			disconnect(1);
		}
	}
	return 0;
}

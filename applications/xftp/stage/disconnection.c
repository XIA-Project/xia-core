#include "stage_utils.h"

#define SCAN_INTERVAL 10
#define MAX_SIZE 1024 * 10

#define INTERFACE1 "wlp6s0 "
#define INTERFACE2 "wlan0 "

#define CONNECT_TIME 16 * 1000
#define DISCONNECT_TIME 4 * 1000

#define GET_SSID_LIST "iwlist wlp6s0 scanning | grep -E '(\\\"[a-zA-Z0-9 _-.]*\\\")|(Signal level=-?[0-9]* dBm)' -o"
#define CLOSE_NETWORK_MANAGER "service network-manager stop"
//#define CONNECT_TO "iwconfig wlp6s0 essid "
//#define GET_CUR_SSID "iwconfig wlp6s0 | grep '\\\"[a-zA-Z0-9 _-.]*\\\"' -o"
//#define GET_DHCP "dhcpcd wlp6s0"

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
	string cmd;

	cmd += "iwconfig ";
	if(interface == 1){
		cmd += INTERFACE1;
	}
	else if(interface == 2){
		cmd += INTERFACE2;
	}
	else
		return -1;
	cmd += "essid off";
say("cmd = %s\n", cmd.c_str());
	result = execSystem(cmd);
	say("disconnect to SSID: %s\n", result.c_str());
	return 0;
}

int connect_SSID(int interface, char * ssid){
say("in connect_SSID\n");
	string result;
	string cmd;

	cmd += "iwconfig ";
	if(interface == 1){
		cmd += INTERFACE1;
	}
	else if(interface == 2){
		cmd += INTERFACE2;
	}
	else
		return -1;
	cmd += "essid ";
	cmd += ssid;
//say("cmd = %s\n", cmd.c_str());
	result = execSystem(cmd);
	say("connect to SSID: %s\n", result.c_str());

	cmd = "dhcpcd ";
	if(interface == 1){
		cmd += INTERFACE1;
	}
	else if(interface == 2){
		cmd += INTERFACE2;
	}
	else
		return -1;
//say("cmd = %s\n", cmd.c_str());
	result = execSystem(string(cmd));
	say("get dhcp done: %s\n",result.c_str());
	return 0;
}

int main(){
	string result;	
	long begin_time, end_time, total_time;
	result = execSystem(CLOSE_NETWORK_MANAGER);
	printf("close_network_manager %s\n", result.c_str());
	//result = execSystem("iwconfig wlan0 essid off");
	result = execSystem("iwconfig wlp6s0 essid off");
	
	while (1) {			
		//ssid_list = execSystem(GET_SSID_LIST);
		//printf("scanning\n");
		int rtn;
		begin_time = now_msec();
		say("-------------connect begin at %ld \n", begin_time);
		rtn = connect_SSID(1, "XIA_Tenda_1");
		usleep(CONNECT_TIME * 1000);
		end_time = now_msec();
		say("-------------connect end at %ld \n", end_time);
		total_time = end_time - begin_time;
		say("-------------connect total time = %ld \n", total_time);
		
		
		begin_time = now_msec();
		say("-------------disconnect begin at %ld \n", begin_time);
		rtn = disconnect_SSID(1);
		usleep(DISCONNECT_TIME * 1000);
		end_time = now_msec();
		say("-------------disconnect end at %ld \n", end_time);
		total_time = end_time - begin_time;
		say("-------------disconnect total time = %ld \n", total_time);
		
		say("\n-----------------------------------------------------------------\n");
	}
	return 0;
}

#include "stage_utils.h"


#define SCAN_INTERVAL 10
#define MAX_SIZE 1024 * 10

#define INTERFACE1 "wlp6s0 "
#define INTERFACE2 "wlp1s0 "

#define GET_SSID_LIST "iwlist wlp1s0 scanning | grep -E '(\\\"[a-zA-Z0-9 _-.]*\\\")|(Signal level=-?[0-9]* dBm)' -o"
#define CLOSE_NETWORK_MANAGER "service network-manager stop"
#define NETWORK_NAME1 "XIA_TPlink_5GHz"
#define NETWORK_NAME2 "XIA_tenda_1"
#define NETWORK_NAME3 ""
#define NETWORK_NAME4 ""

int connect_time = 4;
int disconnect_time = 32;
//#define CONNECT_TO "iwconfig wlp6s0 essid "
//#define GET_CUR_SSID "iwconfig wlp6s0 | grep '\\\"[a-zA-Z0-9 _-.]*\\\"' -o"
//#define GET_DHCP "dhcpcd wlp6s0"

int cast_sig(char * p_sig){
say("in cast_sig\n");
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
say("sig = %d\n",sig * flag);
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

int connect_SSID(int interface, const char * ssid){
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
say("cmd = %s\n", cmd.c_str());
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
say("cmd = %s\n", cmd.c_str());
	result = execSystem(string(cmd));
	say("get dhcp done: %s\n",result.c_str());
	return 0;
}

int main(){
	string ssid_list;
	string result;	
	char p_ssid_list[MAX_SIZE];
	struct timeval t1, t2;
	double elapsedTime;
	
	result = execSystem(CLOSE_NETWORK_MANAGER);
	say("close_network_manager %s\n", result.c_str());
	//result = execSystem("iwconfig wlan0 essid off");
	//result = execSystem("iwconfig wlp6s0 essid off");
	//printf("disconnect %s\n", result.c_str());
	

	printf("input connect time>> ");
	scanf("%d", &connect_time);
	printf("input disconnect time>> ");
	scanf("%d", &disconnect_time);
	
	while (1) {			
			gettimeofday(&t1, NULL);
			char cur_ssid1[20],cur_ssid2[20];
			
			//string curSSID2 = get_SSID(2);
			bzero(cur_ssid1, sizeof(cur_ssid1));
			//bzero(cur_ssid2, sizeof(cur_ssid2));
			
			connect_SSID(1, NETWORK_NAME1);
			string curSSID1 = get_SSID(1);
			while(curSSID1.empty()){
				say("not connecting, connect it!\n");
				connect_SSID(1, NETWORK_NAME1);
				curSSID1 = get_SSID(1);
			}
			
			usleep(connect_time * 1000 * 1000);

			disconnect_SSID(1);
			curSSID1 = get_SSID(1);
			while(!curSSID1.empty()){
				printf("still connecting, disconnect it!\n");
				disconnect_SSID(1);
				curSSID1 = get_SSID(1);
			}
			
			usleep(disconnect_time * 1000 * 1000);
		
		
			connect_SSID(1, NETWORK_NAME2);
			curSSID1 = get_SSID(1);
			while(curSSID1.empty()){
				say("not connecting, connect it!\n");
				connect_SSID(1, NETWORK_NAME2);
				curSSID1 = get_SSID(1);
			}
			
			usleep(connect_time * 1000 * 1000);

			disconnect_SSID(1);
			curSSID1 = get_SSID(1);
			while(!curSSID1.empty()){
				printf("still connecting, disconnect it!\n");
				disconnect_SSID(1);
				curSSID1 = get_SSID(1);
			}
			
			usleep(disconnect_time * 1000 * 1000);
			
		gettimeofday(&t2, NULL);
		
		elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;      // sec to ms
        elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;   // us to ms
		say("current elapsedTime: %f\n", elapsedTime);
		
		say("\n-----------------------------------------------------------------\n");
	}
	return 0;
}

#include "stage_utils.h"


#define SCAN_INTERVAL 10
#define MAX_SIZE 1024 * 10

//#define INTERFACE1 "wlp6s0 "
//#define INTERFACE2 "wlan0 "

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
	string ssid_list;
	string result;	
	char p_ssid_list[MAX_SIZE];
	
	result = execSystem(CLOSE_NETWORK_MANAGER);
	printf("close_network_manager %s\n", result.c_str());
	result = execSystem("iwconfig wlan0 essid off");
	result = execSystem("iwconfig wlp6s0 essid off");
	
	while (1) {			
		ssid_list = execSystem(GET_SSID_LIST);
		printf("scanning\n");
		if (!ssid_list.empty()) {
			strcpy(p_ssid_list, ssid_list.c_str());
			char * saveptr;
			char *sig_strenth = strtok_r(p_ssid_list, "\n", &saveptr);
			char *ssid = strtok_r(NULL, "\n", &saveptr);
			char *ssid1 = NULL, *ssid2 = NULL;
			int sig1 = 0, sig2 = 0;

			if(strncmp(ssid,"\"XIA",4) == 0){
				printf("ssid = %s\n", ssid);
				printf("sig_strenth = %s\n",sig_strenth);
				if(ssid1 == NULL){
					ssid1 = ssid;
					sig1 = cast_sig(sig_strenth);
					printf("signal = %d\n", sig1);
				}
				else{
					ssid2 = ssid;
					sig2 = cast_sig(sig_strenth);
					printf("signal = %d\n", sig2);
				}
			}

			while((sig_strenth = strtok_r(NULL, "\n", &saveptr)) != NULL){
				ssid = strtok_r(NULL, "\n", &saveptr);
				if(strncmp(ssid,"\"XIA",4) == 0){
					say("ssid = %s\n", ssid);
					say("sig_strenth = %s\n",sig_strenth);
					if(ssid1 == NULL){
						ssid1 = ssid;
						sig1 = cast_sig(sig_strenth);
						//printf("signal = %d\n", sig1);
					}
					else{
						ssid2 = ssid;
						sig2 = cast_sig(sig_strenth);
						//printf("signal = %d\n", sig2);
					}
				}
			}

			char cur_ssid1[20],cur_ssid2[20];
			string curSSID1 = get_SSID(1);
			string curSSID2 = get_SSID(2);
			bzero(cur_ssid1, sizeof(cur_ssid1));
			bzero(cur_ssid2, sizeof(cur_ssid2));

			if(!curSSID1.empty()){
				strcpy(cur_ssid1, curSSID1.c_str());
				say("cur_ssid1 = %s\n", cur_ssid1);
				if(	(ssid1 == NULL && ssid2 == NULL) || 
					(ssid1 == NULL && strcmp(cur_ssid1, ssid2) != 0) ||
					(ssid2 == NULL && strcmp(cur_ssid1, ssid1) != 0) ||
					(strcmp(cur_ssid1,ssid1) != 0 && strcmp(cur_ssid1, ssid2) != 0)){
					disconnect_SSID(1);
					curSSID1 = get_SSID(1);
					if(!curSSID1.empty()){
						strcpy(cur_ssid1, curSSID1.c_str());
						say("cur_ssid1 = %s\n",cur_ssid1);
					}
				}
			}
			if(!curSSID2.empty()){
				strcpy(cur_ssid2,curSSID2.c_str());
				say("cur_ssid2 = %s\n",cur_ssid2);
				if(	(ssid1 == NULL && ssid2 == NULL) || 
					(ssid1 == NULL && strcmp(cur_ssid2, ssid2) != 0) ||
					(ssid2 == NULL && strcmp(cur_ssid2, ssid1) != 0) ||
					(strcmp(cur_ssid2,ssid1) != 0 && strcmp(cur_ssid2, ssid2) != 0)){
					disconnect_SSID(2);
					curSSID2 = get_SSID(2);
					if(!curSSID2.empty()){
						strcpy(cur_ssid2,curSSID2.c_str());
						say("cur_ssid2 = %s\n",cur_ssid2);
					}
				}
			}
			

			if(curSSID1.empty()){
				if(curSSID2.empty()){
					if(ssid2 == NULL && ssid1 != NULL){
						connect_SSID(1, ssid1);
					}
					else if(ssid1 != NULL && ssid2 != NULL){
						if(sig1 > sig2){
							connect_SSID(1, ssid1);
							connect_SSID(2, ssid2);
						}
						else{
							connect_SSID(1, ssid2);
							connect_SSID(2, ssid1);
						}
					}
				}
				else if(strcmp(cur_ssid2,ssid1)==0){
					connect_SSID(1, ssid1);
				}
				else if(strcmp(cur_ssid2,ssid2)==0){
					connect_SSID(1, ssid2);
				}
				else{
					die(-1, "2 more new network ssid!\n");
				}
			}
			else if(ssid2 != NULL && curSSID2.empty()){
				if(strcmp(cur_ssid1,ssid1)==0){
					connect_SSID(2, ssid2);
				}
				else if(strcmp(cur_ssid1, ssid2)==0){
					connect_SSID(2, ssid1);
				}
				else{
					die(-1, "2 more new network ssid!\n");
				}
			}
			else if(curSSID1 == curSSID2 && ssid2!=NULL){
				if(strcmp(cur_ssid1,ssid1)==0){
					connect_SSID(2, ssid2);
				}
				else if(strcmp(cur_ssid1, ssid2)==0){
					connect_SSID(2, ssid1);
				}
				else{
					die(-1, "2 more new network ssid!\n");
				}
			}

		}
		
		usleep(SCAN_INTERVAL * 1000);
		say("\n-----------------------------------------------------------------\n");
	}
	return 0;
}

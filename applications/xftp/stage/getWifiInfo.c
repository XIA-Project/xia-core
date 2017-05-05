#include "stage_utils.h"
#include <stdlib.h> 
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#define GET_SSID_LIST "iwlist wlp6s0 scanning"
string cmd = "iwlist ";
ofstream getWifiInfo("getWifiInfo.log");
void getConfig(int argc, char** argv)
{
        int c;
        opterr = 0;

        while ((c = getopt(argc, argv, "i:")) != -1) {
                switch (c) {
                        case 'i':
				printf("optarg = %s\n", optarg);
				cmd += optarg;
                        	break;
                        default:
                                break;
                }
        }
}
int main(int argc, char** argv){
	if (argc!=3) die(-1, "need a interface");
	getConfig(argc, argv);
	string result;
	cmd += " scanning";
	while(true){
		//string result;
		//string cmd = "iwlist wlp6s0 scanning";
		printf("cmd = %s\n", cmd.c_str());
		getWifiInfo << "Time" << now_msec() << endl;
		result = execSystem(cmd);

		getWifiInfo << result << endl;
	}
	return 0;
}

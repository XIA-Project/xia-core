#include "stage_utils.h"
#include <stdlib.h> 
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#define GET_SSID_LIST "iwlist wlp6s0 scanning"
string cmd == "iwlist ";
ofstream getWifiInfo("getWifiInfo.log");
void getConfig(int argc, char** argv)
{
        int c;
        opterr = 0;

        while ((c = getopt(argc, argv, "i:")) != -1) {
                switch (c) {
                        case 'i':
                        	if ((CHUNKSIZE = atoi(optarg)) != 0) {
                            	CHUNKSIZE *= 1024;
                       		}
							cmd += 
                        	break;
                        
                        default:
                                break;
                }
        }
}
int main(int argc, char** argv){
	if (argc!=2) die("need a interface");
	getConfig(argc, argv);
	string result;
	cmd += " scanning";
	while(true){
		//string result;
		//string cmd = "iwlist wlp6s0 scanning";
		result = execSystem(cmd);
		getWifiInfo << result << endl << "time" << now_msec() << endl;
	}
	return 0;
}

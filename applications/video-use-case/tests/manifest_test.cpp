#include <stdlib.h>
#include <cstdio>
#include <map>
#include <vector>
#include "../src/manifest.h"

int main() {
	int status = parse_dash_manifest("../src/resources/dash2/", "../src/resources/dash2/133024.mpd", "../src/resources/dash2/parsed_out.mpd");

	map<string, vector<string> > pathToUrl;
	pathToUrl["../src/resources/dash2/video_0_400000/dash/segment_1.m4s"].push_back("a");
	pathToUrl["../src/resources/dash2/video_0_400000/dash/segment_1.m4s"].push_back("b");
	pathToUrl["../src/resources/dash2/video_0_400000/dash/segment_1.m4s"].push_back("c");
	pathToUrl["../src/resources/dash2/video_0_400000/dash/segment_2.m4s"].push_back("e");
	pathToUrl["../src/resources/dash2/video_0_400000/dash/segment_3.m4s"].push_back("f");
	pathToUrl["../src/resources/dash2/video_0_400000/dash/segment_4.m4s"].push_back("g");

	if(status == 1){
		status = generate_XIA_manifest("../src/resources/dash2/", "../src/resources/dash2/parsed_out.mpd", "../src/resources/dash2/xia_manifest_out.mpd", pathToUrl);
		if(status != 1){
			printf("Failed!\n");
		}
	} else {
		printf("Failed!\n");
	}
	return 0;
}
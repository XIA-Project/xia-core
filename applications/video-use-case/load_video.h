#ifndef __CONTENT_PUBLISHER_H__
#define __CONTENT_PUBLISHER_H__

#include <vector>
#include <string>
#include "Xsocket.h"
#include "Xkeys.h"
#include "dagaddr.hpp"
#include "utils.h"
#include "manifest.h"
#include "xcache.h"

#define VERSION "v1.0"
#define TITLE "XIA Video Publisher"

#define MAX_XID_SIZE 100
#define MAX_DAG_SIZE 512

#define MAX_INPUT_SIZE 1000

using namespace std;

// The full url in manifest is:
// 		www.origin.xia/DAG.0.-.CID$.....

// this is where the video file should be.
static const char* RESOURCE_FOLDER = "resources/";

// name of the generated manifest files
//
// parsed_dash.mpd is the file without SegmentTemplate. It makes
// it more specific. SegmentTemplate doesn't work well with the
// DAG like urls
//
// generated_dash.mpd is the manifest generated with DAG urls based
// on parsed_dash.mpd
static const char* PARSED_DASH_MANIFEST = "parsed_dash.mpd";
static const char* GENERATED_DASH_MANIFEST = "generated_dash.mpd";

typedef struct _ServerVideoInfo {
    string videoName;		// name of the video
    string manifestName;	// name of the manifest

    // cids for manifest file, through right now manifest is not used
    vector<string> manifestUrls;
    // path -> list of dagurls
    map<string, string> dagUrls;

    XcacheHandle xcache;
} ServerVideoInfo;

#endif


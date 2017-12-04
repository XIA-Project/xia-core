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

// name of the CDN to register to nameserver
//
// this name would be used for load balancing by nameserver;
// load balancing works on equivalent set of servers that share
// the name.
static const char* CDN_NAME = "www.cdn1.xia";

// name of the video use case domain on GENI running XIA.
//
// this name is not registered at the nameserver, but rather as an
// hook for the full url in manifest for the multi-CDN use case.
// The full url in manifest is:
//      www.origin.xia/CID?cdn1=CDN1Name&cdn2=CDN2Name
static const char* MULTI_CDN_NAME = "www.origin.xia";

// all the CDN names for the multi-cdn use case
static const char * MUTI_CDN_CDN_NAMES[] = {
    "www.cdn1.xia",
    "www.cdn2.xia",
};

// number of CDNs that host our video use case.
static const unsigned MULTI_CDN_CDN_NUMS = (sizeof(MUTI_CDN_CDN_NAMES) / sizeof(const char *));

// several options for generating manifest with several options
// user should enter to the command line prompt
//  "host": generate plain DAG url so client fetches it directly
//      from DAG
//  "cdn": genereate 'cdn.name/CID' so that nameserver can load
//      balance among a set of serves that has the CID
//  "multicdn": generate "vid.name/CID" so nameserver can load
//      balance among a set of CDN clusters that has the CID
//
//  Note: system now doesn't recognize which CID belongs to which
//  server and CDN. So to get load balancing works correctly, it is
//  best to publish contents on all involving servers/clusters.
static const char* MANIFEST_TO_HOST = "host";
static const char* MANIFEST_TO_CDN = "cdn";
static const char* MANIFEST_TO_MULTI_CDN = "multicdn";

// check if user want to quit command line
static const char* QUIT = "quit";
static const char* Q = "q";

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

static const char *REQUEST_ID_PLACE_HOLDER = "@XXXXXXXXXX@";

typedef struct _ServerVideoInfo {
    string videoName;       //name of the video
    string manifestName;    // name of the manifest

    string manifestType;    // type of manifest host, cdn, multicdn

    // cids for manifest file, through right now manifest is not used
    vector<string> manifestUrls;
    // path -> list of dagurls
    map<string, vector<string> > dagUrls;

    XcacheHandle xcache;
} ServerVideoInfo;

/*
 ** display cmd line options and exit
 */
void help(const char *name);

/*
 ** get the local address
 */
void getLocalAddr();

/*
 ** for debugging mainly. print out all the
 *  information related to current video that is
 *  being published (dagUrls, manifestUrls etc).
 */
void print_dash_info(ServerVideoInfo *videoInfo);

/*
 ** convert the AD:HID to DAG
 *  the resulting DAG is used during load balancing phase
 */
sockaddr_x localAddr2DAG(string AD, string HID);

/**
 * generate the CDN name options in the url for the multi-CDN
 * use case.
 */
string generateCDNOptionsUrl();

/*
 ** publish a given content with video name
 *      publish CIDs and generate manifest
 */
int publish_content(const char* name);

/*
 ** publish the dash videos
 */
int process_dash_content(const char* name);

/*
 ** process the files in the given bitrate directory
 */
void process_dash_files(const char* dir);

/*
 ** publish each video segment with the video name
 *  append it in the dag url lists
 */
void publish_each_segment_with_name(const char* name, vector<string> & dagUrls);

/*
 ** create the XIA video manifest based on the video name and
 *  dag urls associated with the name
 */
int create_xia_dash_manifest(ServerVideoInfo *videoInfo);

/*
 ** save the manifest DAG url to disk so that video server
 *  can have this information.
 *
 *  Note for now video server need to be ran AFTER publishing
 *  all the content
 */
int create_xia_dash_manifest_urls(ServerVideoInfo *videoInfo);

/*
 ** publish the generated manifest files with given video name
 */
int publish_manifest(const char* name);

#endif

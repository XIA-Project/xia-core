#include <stdio.h>
#include <vector>
#include <dirent.h>
#include <stdarg.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <assert.h>
#include <dirent.h>
#include "video_publisher.h"

using namespace std;

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char my4ID[MAX_XID_SIZE];

ServerVideoInfo videoInfo;

/**
 * get origina or cdn option
 * @param name file name for this excutable
 */
void help(const char *name) {
    printf("\n%s (%s)\n", TITLE, VERSION);
    printf("usage: %s origin/cdn\n", name);
    printf("where:\n");
    printf(" if it is publisher in origin domain, type 'origin', else type 'cdn'\n");
    printf("\n");
}

/**
 * Fill myAD, myHID and my4ID from local dag info
 */
void getLocalAddr() {
    int sock;
    char dag[MAX_DAG_SIZE];

    // create a socket
    if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
        die(-1, "Unable to create the XIA socket\n");
    }

    // read the localhost AD and HID
    if (XreadLocalHostAddr(sock, dag, MAX_DAG_SIZE, my4ID, MAX_XID_SIZE) < 0 ) {
        die(-1, "Reading localhost address\n");
    }

    // Graph: build DAGs for use as addresses in XIA
    Graph g_localhost(dag);

    // Return the AD and HID for the Graph's intent node
    strcpy(myAD, g_localhost.intent_AD_str().c_str());
    strcpy(myHID, g_localhost.intent_HID_str().c_str());
}

void print_dash_info(ServerVideoInfo* videoInfo) {
    say("video info: \n");
    say("video name: %s\n", videoInfo->videoName.c_str());

    for (unsigned int i = 0; i < videoInfo->manifestUrls.size(); i++) {
        say("video manifest urls: %s\n", videoInfo->manifestUrls[i].c_str());
    }

    for (map<string, vector<string> >::iterator it = videoInfo->dagUrls.begin(); it != videoInfo->dagUrls.end(); ++it) {
        say("video segment uri: %s\n", it->first.c_str());

        for (unsigned i = 0; i < it->second.size(); ++i) {
            say("video segment cid url: %s\n", it->second[i].c_str());
        }
    }
}

/**
 * Convert string AD and HID to sockaddr_x dag
 */

sockaddr_x localAddr2DAG(string AD, string HID) {
    sockaddr_x addr;

    Node n_src;
    // pick the string after ':'
    Node n_ad(XID_TYPE_AD, strchr(AD.c_str(), ':') + 1);
    Node n_hid(XID_TYPE_HID, strchr(HID.c_str(), ':') + 1);

    Graph gAddr = n_src * n_ad * n_hid;
    gAddr.fill_sockaddr(&addr);

    return addr;
}

string generateCDNOptionsUrl() {
    string result;

    for (unsigned i = 0; i < MULTI_CDN_CDN_NUMS; i++) {
        stringstream ss;
        ss << (i + 1);

        result += "cdn" + ss.str() + "=" + MUTI_CDN_CDN_NAMES[i];

        if (i != MULTI_CDN_CDN_NUMS - 1) {
            result += "&";
        }
    }

    return result;
}

int publish_content(ServerVideoInfo* videoInfo) {
    char videoPathName[MAX_PATH_SIZE];
    memset(videoPathName, '\0', sizeof(videoPathName));

    // if input video name is "dash1"
    // the generate pathname will be resources/dash1
    strcat(videoPathName, RESOURCE_FOLDER);
    strcat(videoPathName, videoInfo->videoName.c_str());

    // clear out the old data in previously published videos
    videoInfo->manifestUrls.clear();
    videoInfo->dagUrls.clear();

    if (process_dash_content(videoPathName)) {
        return -1;
    }
    create_xia_dash_manifest(videoInfo);
    // videoInfo->manifestName == "generated_dash.mpd"
    publish_manifest(videoInfo->manifestName.c_str());
    create_xia_dash_manifest_urls(videoInfo);
    return 0;
}

/**
 * Puts files into Xcache, and generates dags urls for each chunk spliting from
 * file
 * @param  name the name of file that will be splited and cached
 * @return      1 as succeeds
 *              -1 as failure
 */
int process_dash_content(const char *name) {
    int n;
    struct dirent **namelist;
    DIR *dir;

    // the DASH video must be in a directory
    if (!(dir = opendir(name))) {
        warn("Unable to open video directory: %s\n", name);
        return -1;
    }

    // version sort gives order of:
    // abc1
    // abc2
    // abc3
    // abc11
    // abc22
    // abc33
    // scandir scans the directory 'name' and malloc memory,
    // collects sub directory in namelist
    n = scandir(name, &namelist, 0, versionsort);

    if (n < 0) {
        warn("Unable to read video directory: %s\n", name);
        return -1;
    } else {
        // namelist now contains:
        // .
        // ..
        // 480_400000
        // 480_800000
        // 480_1200000
        for (int i = 0 ; i < n; i++) {
            char path[MAX_PATH_SIZE];
            // name: resources/dash1
            // namelist[i]->d_name: 480_400000, 480_800000, 480_1200000
            // path: resources/dash1/480_400000/
            int len = snprintf(path, sizeof(path) - 1, "%s/%s/dash", name, namelist[i]->d_name);
            path[len] = 0;

            // if it is not '.' or '..'
            if (strcmp(namelist[i]->d_name, ".") != 0 && strcmp(namelist[i]->d_name, "..") != 0) {
                process_dash_files(path);
            }
            free(namelist[i]);
        }
        free(namelist);
    }
    closedir(dir);
    return 0;
}

void process_dash_files(const char* dir) {
    int n;
    struct dirent **namelist;
    n = scandir(dir, &namelist, 0, versionsort);

    // namelist now contains:
    // init.map4, segment_X.m4s
    if (n > 0) {
        for (int i = 0 ; i < n; i++) {
            char path[MAX_PATH_SIZE];
            // path: resources/dash1/480_400000/segment_X.m4s or init.map4
            int len = snprintf(path, sizeof(path) - 1, "%s/%s", dir, namelist[i]->d_name);
            path[len] = 0;

            if (strcmp(namelist[i]->d_name, ".") != 0 && strcmp(namelist[i]->d_name, "..") != 0) {
                vector<string> urlsForSegment;
                publish_each_segment_with_name(path, urlsForSegment);

                if (urlsForSegment.size() > 0) {
                    // map a file path to a list of chunk's dags from this file
                    videoInfo.dagUrls[path] = urlsForSegment;
                }
            }
            free(namelist[i]);
        }
        free(namelist);
    }
}

/**
 * put video into Xcache, and generate corresponding dag, put it into dagUrls
 * @param name    the name of file that will be put into Xcache
 * @param dagUrls container of dags
 */
void publish_each_segment_with_name(const char* name, vector<string> & dagUrls) {
    int count;
    sockaddr_x *addrs = NULL;

    // split resources/dash1/480_400000/segment_X.m4s into CHUNKSIZE chunks
    // and cache it into Xcache
    // a list of dags for each chunk will be returned in addrs
    if ((count = XputFile(&videoInfo.xcache, name, CHUNKSIZE, &addrs)) < 0) {
        warn("cannot put file for %s\n", name);
        return;
    }

    // push back the cids
    for (int i = 0; i < count; i++) {
        Graph g;
        g.from_sockaddr(&addrs[i]);
        Node intent_node = g.get_final_intent();

        if (videoInfo.manifestType == MANIFEST_TO_HOST) {
            // http://DAG.3.0.-.AD$c26124c5c3c3a964c03693c4bdfb625bb21faa1f.
            // 1.-.HID$117a77f63181b5d6e63d5288d00cc09131462df5.
            // 2.-.SID$15b7347ac78fc9aded44fbca0e6243698144fedd.
            // 3.-.CID$2b975af6f12be497701a01ef5da7f4a13fb93402
            dagUrls.push_back(g.http_url_string());
        } else if (videoInfo.manifestType == MANIFEST_TO_CDN) {
            // http://www.cdn1.xia/CID:2b975af6f12be497701a01ef5da7f4a13fb93402
            dagUrls.push_back("http://" + string(REQUEST_ID_PLACE_HOLDER) + string(CDN_NAME) + "/" + intent_node.to_string());
        } else if (videoInfo.manifestType == MANIFEST_TO_MULTI_CDN) {
            // http://www.origin.xia/CID:b135bc5d27e10fc7d997c0373f192f5f6c74e3b7?cdn1=www.cdn1.xia&amp;cdn2=www.cdn2.xia
            dagUrls.push_back("http://" + string(REQUEST_ID_PLACE_HOLDER) + string(MULTI_CDN_NAME) + "/" + intent_node.to_string() + "?" + generateCDNOptionsUrl());
        }
    }
}

/**
 * parse the original manifest template file to explictly listed manifest file,
 * each item in the list contains path name to the final video file
 * @param  videoInfo
 * @return      1 as succeeds
 *              -1 as failure
 */
int create_xia_dash_manifest(ServerVideoInfo *videoInfo) {
    int rc;
    // manifestName: "xia_manifest_dash1.mpd"
    // MANIFEST_EXTENSION is in video_use_case/utils.h
    string manifestName = "xia_manifest_" + videoInfo->videoName + "." + MANIFEST_EXTENSION;

    // fullPath: "resources/dash1/"
    string fullPath = RESOURCE_FOLDER;
    fullPath += videoInfo->videoName + "/";

    // parsed: "resources/dash1/parsed_dash.mpd"
    string parsed = fullPath + PARSED_DASH_MANIFEST;
    // generated: "generated/dash1/generated_dash.mpd"
    string generated = fullPath + GENERATED_DASH_MANIFEST;

    // remove existing manifest files
    remove(parsed.c_str());
    remove(generated.c_str());

    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir (fullPath.c_str())) != NULL) {
        while ((ent = readdir (dir)) != NULL) {
            // try to find 155742.mpd to open
            if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0
                    && strstr(ent->d_name, ".mpd") != NULL) {
                char path[MAX_PATH_SIZE];
                // path: resources/dash1/155742.mpd
                int len = snprintf(path, sizeof(path) - 1, "%s%s", fullPath.c_str(), ent->d_name);
                path[len] = 0;

                string original = path;
                // fullPath: resources/dash1/
                // original: resources/dash1/155742.mpd
                // parsed: resources/dash1/parsed_dash.mpd
                if ((rc = parse_dash_manifest(fullPath.c_str(), original.c_str(), parsed.c_str())) < 0) {
                    warn("failed to create the video manifest file\n");
                    return -1;
                }
                // fullPath: resources/dash1/
                // parsed: resources/dash1/parsed_dash.mpd
                // generated: generated/dash1/generated_dash.mpd
                if ((rc = generate_XIA_manifest(fullPath.c_str(), parsed.c_str(), generated.c_str(), videoInfo->dagUrls)) < 0) {
                    warn("failed to create the video manifest file\n");
                    return -1;
                }
                videoInfo->manifestName = generated;
            }
        }
        closedir (dir);
    } else {
        perror ("cannot open directory");
        return -1;
    }

    return 1;
}

/**
 * put all the dag string of manifest file into a txt file
 * @param  videoInfo
 * @return           -1 or 1
 */
int create_xia_dash_manifest_urls(ServerVideoInfo *videoInfo) {
    // this file will be then used by mnifest_server to locate the actual file
    // location
    string manifestUrlName = "xia_manifest_urls_" + videoInfo->videoName + ".txt";

    FILE *f = fopen(manifestUrlName.c_str(), "w");
    if (f == NULL) {
        printf("Error opening file!\n");
        return -1;
    }

    for (unsigned int i = 0; i < videoInfo->manifestUrls.size(); i++) {
        fprintf(f, "%s\n", videoInfo->manifestUrls[i].c_str());
    }
    fclose(f);

    return 1;
}

/**
 * put the dag-format manifest file into Xcache
 * @param  name the name of manifest file
 * @return      1 or -1
 */
int publish_manifest(const char* name) {
    int count;
    sockaddr_x *addrs = NULL;

    if ((count = XputFile(&videoInfo.xcache, name, CHUNKSIZE, &addrs)) < 0) {
        warn("cannot put file for %s\n", name);
        return -1;
    }

    for (int i = 0; i < count; i++) {
        Graph g;
        g.from_sockaddr(&addrs[i]);

        videoInfo.manifestUrls.push_back(g.http_url_string());
    }

    return 1;
}

int main(int argc, char *argv[]) {
    char input[MAX_INPUT_SIZE];
    char videoName[MAX_INPUT_SIZE];
    char manifestType[MAX_INPUT_SIZE];
    int count;

    // command line parsing and bootstrapping
    // basename, a linux call, break a null-terminated pathname string into
    // directory and filename components
    // path: "/usr/lib", dirname: "/usr", basename: "lib"
    help(basename(argv[0]));
    if (argc != 2) {
        return -1;
    }

    // Prepare an XcacheHandle for use.
    XcacheHandleInit(&videoInfo.xcache);
    getLocalAddr();

    // type == origin or cdn
    char* type = argv[1];
    // if type is "cdn"
    if (strcmp(type, "cdn") == 0) {
        sockaddr_x addr = localAddr2DAG(myAD, myHID);

        // register server DAG to CDN name
        // register www.cdn1.xia to dag address addr
        // then any entity that look up for www.cdn1.xia
        // will get address to it
        if (XregisterAnycastName(CDN_NAME, &addr) < 0) {
            die(-1, "cannot register anycast name\n");
        }
    }
    // if type is "origin"
    // do not register the name to nameserver, because the manifest
    // contains dag path route to itself

    // finding user input1
    say("[Enter [videoName manifestType] to publish a given video]\n");
    say("[Enter q or quit to turn off content publisher]\n");
    while (true) {
        printf(">> ");
        fflush(stdout);

        bzero(input, sizeof(input));
        bzero(videoName, sizeof(videoName));
        bzero(manifestType, sizeof(manifestType));

        ssize_t incount;
        incount = read(STDIN_FILENO, input, sizeof(input));

        // Skip processing on error or user just entering newline
        if (incount < 2) {
            continue;
        }

        // Did the user ask to quit?
        if (strncmp(input, QUIT, 4) == 0
                || strncmp(input, Q, 1) == 0) {
            break;
        }

        if ((count = sscanf(input, "%s %s\n", videoName, manifestType)) < 0) {
            warn("Error reading in user input. Exit\n");
            break;
        }

        if (count != 2) {
            warn("Usage: videoName manifestType\n");
            continue;
        }

        if (count == 2 && strcmp(videoName, QUIT) != 0
                && strcmp(videoName, Q) != 0) {
            // publish content based on input
            videoInfo.videoName = videoName;
            if (strcmp(manifestType, MANIFEST_TO_HOST) != 0
                    && strcmp(manifestType, MANIFEST_TO_CDN) != 0
                    && strcmp(manifestType, MANIFEST_TO_MULTI_CDN) != 0) {
                warn("Invalid manifestType. Defaulting to: host\n");
                videoInfo.manifestType = MANIFEST_TO_HOST;
            } else {
                videoInfo.manifestType = manifestType;
            }
            publish_content(&videoInfo);
        }

    }
    say("[Exit content publisher, clean up resources]\n");

    return 0;
}

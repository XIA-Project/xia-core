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

void help(const char *name) {
    printf("\n%s (%s)\n", TITLE, VERSION);
    printf("usage: %s origin/cdn\n", name);
    printf("where:\n");
    printf(" if it is publisher in origin domain, type 'origin', else type 'cdn'\n");
    printf("\n");
}

void getLocalAddr() {
    int sock;
    char dag[MAX_DAG_SIZE];

    // create a socket, and listen for incoming connections
    if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0){
        die(-1, "Unable to create the XIA socket\n");
    }

    // read the localhost AD and HID
    if (XreadLocalHostAddr(sock, dag, MAX_DAG_SIZE, my4ID, MAX_XID_SIZE) < 0 ){
        die(-1, "Reading localhost address\n");
    }

    Graph g_localhost(dag);
    strcpy(myAD, g_localhost.intent_AD_str().c_str());
    strcpy(myHID, g_localhost.intent_HID_str().c_str());
}

void print_dash_info(ServerVideoInfo* videoInfo){
    say("video info: \n");
    say("video name: %s\n", videoInfo->videoName.c_str());

    for (unsigned int i = 0; i < videoInfo->manifestUrls.size(); i++){
        say("video manifest urls: %s\n", videoInfo->manifestUrls[i].c_str());
    }

    for (map<string, vector<string> >::iterator it = videoInfo->dagUrls.begin(); it != videoInfo->dagUrls.end(); ++it){
        say("video segment uri: %s\n", it->first.c_str());

        for (unsigned i = 0; i < it->second.size(); ++i) {
            say("video segment cid url: %s\n", it->second[i].c_str());
        }
    }
}

sockaddr_x localAddr2DAG(string AD, string HID){
    sockaddr_x addr;

    Node n_src;
    Node n_ad(XID_TYPE_AD, strchr(AD.c_str(), ':') + 1);
    Node n_hid(XID_TYPE_HID, strchr(HID.c_str(), ':') + 1);

    Graph gAddr = n_src * n_ad * n_hid;
    gAddr.fill_sockaddr(&addr);

    return addr;
}

string generateCDNOptionsUrl(){
    string result;

    for(unsigned i = 0; i < MULTI_CDN_CDN_NUMS; i++){
        stringstream ss;
        ss << (i+1);

        result += "cdn" + ss.str() + "=" + MUTI_CDN_CDN_NAMES[i];

        if(i != MULTI_CDN_CDN_NUMS - 1){
            result += "&";
        }
    }

    return result;
}

int publish_content(ServerVideoInfo* videoInfo){
    char videoPathName[MAX_PATH_SIZE];
    memset(videoPathName, '\0', sizeof(videoPathName));

    strcat(videoPathName, RESOURCE_FOLDER);
    strcat(videoPathName, videoInfo->videoName.c_str());

    // clear out the old data in previously published videos
    videoInfo->manifestUrls.clear();
    videoInfo->dagUrls.clear();

    if(process_dash_content(videoPathName)) {
        return -1;
    }
    create_xia_dash_manifest(videoInfo);
    publish_manifest(videoInfo->manifestName.c_str());
    create_xia_dash_manifest_urls(videoInfo);
    return 0;
}

int process_dash_content(const char *name){
    int n;
    struct dirent **namelist;
    DIR *dir;

    // the DASH video must be in a directory
    if (!(dir = opendir(name))){
        warn("Unable to open video directory: %s\n", name);
        return -1;
    }

    n = scandir(name, &namelist, 0, versionsort);

    if (n < 0){
        warn("Unable to read video directory: %s\n", name);
        return -1;
    } else {
        for(int i =0 ; i < n; i++){
            char path[MAX_PATH_SIZE];
            int len = snprintf(path, sizeof(path)-1, "%s/%s/dash", name, namelist[i]->d_name);
            path[len] = 0;

            if (strcmp(namelist[i]->d_name, ".") != 0 && strcmp(namelist[i]->d_name, "..") != 0){
                process_dash_files(path);
            }
            free(namelist[i]);
        }
        free(namelist);
    }
    closedir(dir);
    return 0;
}

void process_dash_files(const char* dir){
    int n;
    struct dirent **namelist;
    n = scandir(dir, &namelist, 0, versionsort);

    if (n > 0) {
        for(int i =0 ; i < n; i++){
            char path[MAX_PATH_SIZE];
            int len = snprintf(path, sizeof(path)-1, "%s/%s", dir, namelist[i]->d_name);
            path[len] = 0;

            if (strcmp(namelist[i]->d_name, ".") != 0 && strcmp(namelist[i]->d_name, "..") != 0){
                vector<string> urlsForSegment;
                publish_each_segment_with_name(path, urlsForSegment);

                if (urlsForSegment.size() > 0){
                    videoInfo.dagUrls[path] = urlsForSegment;
                }
            }
            free(namelist[i]);
        }
        free(namelist);
    }
}

void publish_each_segment_with_name(const char* name, vector<string> & dagUrls){
    int count;
    sockaddr_x *addrs = NULL;

    if ((count = XputFile(&videoInfo.xcache, name, CHUNKSIZE, &addrs)) < 0) {
        warn("cannot put file for %s\n", name);
        return; 
    }

    // push back the cids
    for(int i =0; i < count; i++){
        Graph g;
        g.from_sockaddr(&addrs[i]);
        Node intent_node = g.get_final_intent();

        if(videoInfo.manifestType == MANIFEST_TO_HOST){
            dagUrls.push_back(g.http_url_string()); 
        } else if (videoInfo.manifestType == MANIFEST_TO_CDN) {
            dagUrls.push_back("http://" + string(CDN_NAME) + "/" + intent_node.to_string());
        } else if (videoInfo.manifestType == MANIFEST_TO_MULTI_CDN) {
            dagUrls.push_back("http://" + string(MULTI_CDN_NAME) + "/" + intent_node.to_string() + "?" + generateCDNOptionsUrl());
        }
    }
}

int create_xia_dash_manifest(ServerVideoInfo *videoInfo){
    int rc;
    string manifestName ="xia_manifest_" + videoInfo->videoName + "." + MANIFEST_EXTENSION;

    string fullPath = RESOURCE_FOLDER;
    fullPath += videoInfo->videoName + "/";

    string parsed = fullPath + PARSED_DASH_MANIFEST;
    string generated = fullPath + GENERATED_DASH_MANIFEST;

    // remove existing manifest files
    remove(parsed.c_str());
    remove(generated.c_str());

    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir (fullPath.c_str())) != NULL) {
        while ((ent = readdir (dir)) != NULL) {
            if(strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0
                    && strstr(ent->d_name, ".mpd") != NULL){
                char path[MAX_PATH_SIZE];
                int len = snprintf(path, sizeof(path)-1, "%s%s", fullPath.c_str(), ent->d_name);
                path[len] = 0;
    
                string original = path;

                if ((rc = parse_dash_manifest(fullPath.c_str(), original.c_str(), parsed.c_str())) < 0){
                    warn("failed to create the video manifest file\n");
                    return -1;
                } 
            
                if ((rc = generate_XIA_manifest(fullPath.c_str(), parsed.c_str(), generated.c_str(), videoInfo->dagUrls)) < 0){
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

int create_xia_dash_manifest_urls(ServerVideoInfo *videoInfo){
    string manifestUrlName ="xia_manifest_urls_" + videoInfo->videoName + ".txt";

    FILE *f = fopen(manifestUrlName.c_str(), "w");
    if (f == NULL) {
        printf("Error opening file!\n");
        return -1;
    }

    for (unsigned int i = 0; i < videoInfo->manifestUrls.size(); i++){
        fprintf(f, "%s\n", videoInfo->manifestUrls[i].c_str());
    }
    fclose(f);
        
    return 1;
}

int publish_manifest(const char* name){
    int count;
    sockaddr_x *addrs = NULL;

    if ((count = XputFile(&videoInfo.xcache, name, CHUNKSIZE, &addrs)) < 0) {
        warn("cannot put file for %s\n", name);
        return -1; 
    }

    for(int i =0; i < count; i++){
        Graph g;
        g.from_sockaddr(&addrs[i]);

        videoInfo.manifestUrls.push_back(g.http_url_string()); 
    }

    return 1;
}

int main(int argc, char *argv[]){
    char input[MAX_INPUT_SIZE];
    char videoName[MAX_INPUT_SIZE];
    char manifestType[MAX_INPUT_SIZE];
    int count;

    // command line parsing and bootstrapping
    help(basename(argv[0]));
    if(argc != 2){
        return -1;
    }

    XcacheHandleInit(&videoInfo.xcache);
    getLocalAddr();

    char* type = argv[1];
    if(strcmp(type, "cdn") == 0){
        sockaddr_x addr = localAddr2DAG(myAD, myHID);

        // register server DAG to CDN name
        if(XregisterAnycastName(CDN_NAME, &addr) < 0){
            die(-1, "cannot register anycast name\n");
        }
    }

    // finding user input1
    say("[Enter [videoName manifestType] to publish a given video]\n");
    say("[Enter q or quit to turn off content publisher]\n");
    while(true) {
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

        if ((count = sscanf(input, "%s %s\n", videoName, manifestType)) < 0){
            warn("Error reading in user input. Exit\n");
            break;
        }

        if (count != 2) {
            warn("Usage: videoName manifestType\n");
            continue;
        }

        if(count == 2 && strcmp(videoName, QUIT) != 0
                && strcmp(videoName, Q) != 0){
            // publish content based on input
            videoInfo.videoName = videoName;
            if (strcmp(manifestType, MANIFEST_TO_HOST) != 0
                    && strcmp(manifestType, MANIFEST_TO_CDN) != 0
                    && strcmp(manifestType, MANIFEST_TO_MULTI_CDN) != 0){
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

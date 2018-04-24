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
#include "load_video.h"

using namespace std;

ServerVideoInfo videoInfo;

void help(const char *name) {
    printf("\n%s (%s)\n", TITLE, VERSION);
    printf("usage: %s video [video...]\n", name);
    printf("where:\n");
    printf(" video is just the basname of the video\n\n");
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



void publish_each_segment_with_name(const char* name, vector<string> & dagUrls) {
    int count;
    sockaddr_x *addrs = NULL;

    if ((count = XputFile(&videoInfo.xcache, name, CHUNKSIZE, &addrs)) < 0) {
        warn("cannot put file for %s\n", name);
        return;
    }

    // push back the cids
    for(int i =0; i < count; i++) {
		// strip every but the CID from the dag, we only want the CID in the manifest
		// the proxy will will fill out the complete dag with either the current best cdn,
		// or if it doesn't exist, the AD/HID or the manifest server
        Graph g;
        g.from_sockaddr(&addrs[i]);
		Node cid = g.intent_CID();

		g = Node() * cid;
        dagUrls.push_back(g.http_url_string());
    }
}



void process_dash_files(const char* dir) {
    int n;
    struct dirent **namelist;
    n = scandir(dir, &namelist, 0, versionsort);

    if (n > 0) {
        for(int i =0 ; i < n; i++) {
            char path[MAX_PATH_SIZE];
            int len = snprintf(path, sizeof(path)-1, "%s/%s", dir, namelist[i]->d_name);
            path[len] = 0;

            if (strcmp(namelist[i]->d_name, ".") != 0 && strcmp(namelist[i]->d_name, "..") != 0) {
                vector<string> urlsForSegment;
                publish_each_segment_with_name(path, urlsForSegment);

                if (urlsForSegment.size() > 0) {
                    videoInfo.dagUrls[path] = urlsForSegment;
                }
            }
            free(namelist[i]);
        }
        free(namelist);
    }
}



int process_dash_content(const char *name) {
    int n;
    struct dirent **namelist;
    DIR *dir;

    // the DASH video must be in a directory
    if (!(dir = opendir(name))) {
        warn("Unable to open video directory: %s\n", name);
        return -1;
    }

    n = scandir(name, &namelist, 0, versionsort);

    if (n < 0) {
        warn("Unable to read video directory: %s\n", name);
        return -1;
    } else {
        for(int i =0 ; i < n; i++) {
            char path[MAX_PATH_SIZE];
            int len = snprintf(path, sizeof(path)-1, "%s/%s/dash", name, namelist[i]->d_name);
            path[len] = 0;

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



int create_xia_dash_manifest(ServerVideoInfo *videoInfo) {
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
                    && strstr(ent->d_name, ".mpd") != NULL) {
                char path[MAX_PATH_SIZE];
                int len = snprintf(path, sizeof(path)-1, "%s%s", fullPath.c_str(), ent->d_name);
                path[len] = 0;

                string original = path;

                if ((rc = parse_dash_manifest(fullPath.c_str(), original.c_str(), parsed.c_str())) < 0) {
                    warn("failed to create the video manifest file\n");
                    return -1;
                }

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



int create_xia_dash_manifest_urls(ServerVideoInfo *videoInfo) {
    string manifestUrlName ="xia_manifest_urls_" + videoInfo->videoName + ".txt";

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



int publish_manifest(const char* name) {
    int count;
    sockaddr_x *addrs = NULL;

    if ((count = XputFile(&videoInfo.xcache, name, CHUNKSIZE, &addrs)) < 0) {
        warn("cannot put file for %s\n", name);
        return -1;
    }

    for(int i =0; i < count; i++) {
        Graph g;
        g.from_sockaddr(&addrs[i]);

        videoInfo.manifestUrls.push_back(g.http_url_string());
    }

    return 1;
}



int publish_content(ServerVideoInfo* videoInfo) {
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



int main(int argc, char **argv)
{
    if (argc < 2) {
		help(basename(argv[0]));
        return -1;
    }

    XcacheHandleInit(&videoInfo.xcache);

	for (int i = 1; i < argc; i++) {
        videoInfo.videoName = argv[i];
        publish_content(&videoInfo);
	}

    return 0;
}

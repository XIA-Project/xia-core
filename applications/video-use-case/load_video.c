#include <ftw.h>
#include <stdio.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <dirent.h>
#include "load_video.h"

ServerVideoInfo videoInfo;
int ramdomize = 100;

void help(const char *name) {
    printf("\n%s (%s)\n", TITLE, VERSION);
    printf("usage: %s [-v] [-r <random>] video [video...]\n", name);
    printf("where:\n");
	printf("  -v:        : print version info\n");
	printf("  -r <random>: percentage of segments to randomly load\n");
    printf("video is just the basname of the video\n\n");
	exit(1);
}


void print_dash_info(ServerVideoInfo* videoInfo) {
    say("video info: \n");
    say("video name: %s\n", videoInfo->videoName.c_str());

    for (unsigned int i = 0; i < videoInfo->manifestUrls.size(); i++) {
        say("video manifest urls: %s\n", videoInfo->manifestUrls[i].c_str());
    }

    for (map<string, string>::iterator it = videoInfo->dagUrls.begin(); it != videoInfo->dagUrls.end(); ++it) {
        say("video segment uri: %s\n", it->first.c_str());
        say("video segment cid url: %s\n", it->second.c_str());
    }
}


int create_xia_dash_manifest(ServerVideoInfo *videoInfo) {
    string fullPath = RESOURCE_FOLDER;
    fullPath += videoInfo->videoName + "/";

    string parsed = fullPath + PARSED_DASH_MANIFEST;
    string generated = fullPath + GENERATED_DASH_MANIFEST;

    // remove existing manifest files
    remove(parsed.c_str());
    remove(generated.c_str());

    DIR *dir;
    if ((dir = opendir (fullPath.c_str())) != NULL) {
		struct dirent *ent;

        while ((ent = readdir (dir)) != NULL) {

            if(strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0
                    && strstr(ent->d_name, ".mpd") != NULL) {
                char path[MAX_PATH_SIZE];
                int len = snprintf(path, sizeof(path)-1, "%s%s", fullPath.c_str(), ent->d_name);
                path[len] = 0;

                string original = path;

                if (parse_dash_manifest(fullPath.c_str(), original.c_str(), parsed.c_str()) < 0) {
                    warn("failed to create the video manifest file\n");
                    return -1;
                }

                if (generate_XIA_manifest(fullPath.c_str(), parsed.c_str(), generated.c_str(), videoInfo->dagUrls) < 0) {
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
    string manifestUrlName ="resources/xia_manifest_urls_" + videoInfo->videoName + ".txt";

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

    for(int i = 0; i < count; i++) {
        Graph g;
        g.from_sockaddr(&addrs[i]);

        videoInfo.manifestUrls.push_back(g.http_url_string());
    }

    return 1;
}


static int process_file(const char *fpath, const struct stat *, int tflag, struct FTW *)
{
    int count;
    sockaddr_x *addrs = NULL;

	int r = random() % 100;

	if (r > ramdomize) {
		printf("skipping: %s\n", fpath);
		return 0;
	} else {
		printf("chunking: %s\n", fpath);
	}

	if (tflag != FTW_F) {
		// this shouldn't happen, but check just to be safe
		return 0;
	}
	if (strcasestr(fpath, ".mpd") != NULL) {
		// don't chunk the manifest right now
		return 0;
	}

    count = XputFile(&videoInfo.xcache, fpath, CHUNKSIZE, &addrs);
	if (count > 1) {
		printf("error: segment %s too large to fit in a chunk\n", fpath);
		return -1;

	} else if (count <= 0) {
		printf("unable to chunk segment %s\n", fpath);
		return -1;
	}

	// strip everything but the CID from the dag, we only want the CID in the manifest
	// the proxy will will fill out the complete dag with either the current best cdn,
	// or if it doesn't exist, the AD/HID or the manifest server
    Graph g;
    g.from_sockaddr(&addrs[0]);
	Node cid = g.intent_CID();

	g = Node() * cid;
    videoInfo.dagUrls[fpath] = g.http_url_string();

	return 0;
}




int publish_content(ServerVideoInfo* videoInfo)
{
    char videoPathName[MAX_PATH_SIZE];

	sprintf(videoPathName, "%s%s", RESOURCE_FOLDER, videoInfo->videoName.c_str());

    // clear out the old data in previously published videos
    videoInfo->manifestUrls.clear();
    videoInfo->dagUrls.clear();

	// chunk the seegments and save path info for manifest
	if (nftw(videoPathName, process_file, 25, 0)) {
		printf("error!\n");
		return -1;
	}

	printf("creating manifest for %s\n", videoInfo->videoName.c_str());
    create_xia_dash_manifest(videoInfo);
    publish_manifest(videoInfo->manifestName.c_str());
    create_xia_dash_manifest_urls(videoInfo);
    return 0;
}



int main(int argc, char **argv)
{
	char c;

	while ((c = getopt(argc, argv, "vr:")) != -1) {
		switch (c) {
			case 'v':
				printf("\n%s (%s)\n", TITLE, VERSION);
				return 0;
				break;
			case 'r':
				ramdomize = MIN(100, MAX(1, atoi(optarg)));
				break;
			case '?':
			default:
				// Help Me!
				help(basename(argv[0]));
				exit(1);
				break;
		}
	}

	if (optind == argc) {
		help(basename(argv[0]));
		return 1;
	}

	srandom(time(NULL));
    XcacheHandleInit(&videoInfo.xcache);

	for (int i = optind; i < argc; i++) {
        videoInfo.videoName = argv[i];
        publish_content(&videoInfo);
	}

    return 0;
}

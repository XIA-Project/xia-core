#include <stdio.h>
#include <string.h>
#include <sstream>
#include <string>
#include <map>
#include <algorithm>
#include <dirent.h>
#include "utils.h"
#include "manifest.h"

map<string, string> attrToVal;

/**
 * segment_$Number$.m4s
 * pick $Number$ out, and compare them
 */
struct sort_on_segment_id {
    inline bool operator() (const string& s1, const string& s2) {
        size_t start1 = s1.find_last_of("_");
        size_t end1 = s1.find_last_of(".");
        size_t start2 = s2.find_last_of("_");
        size_t end2 = s2.find_last_of(".");

        int s1Int = atoi(s1.substr(start1 + 1, end1 - start1 - 1).c_str());
        int s2Int = atoi(s2.substr(start2 + 1, end2 - start2 - 1).c_str());

        return s1Int < s2Int;
    }
};

/**
 * return a vector of all the relative paths for segment file(segment_X.m4s)
 */
static vector<string> find_all_segment_url(const char *video_folder, string & path) {
    vector<string> result;
    // rescourse/dash/480_1200000/dash/
    string fullPath = video_folder + path;

    // this path contains:
    // .
    // ..
    // init.mp4
    // segment_1.m4s
    // segment_2.m4s
    // segment_X.m4s
    // ...
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir (fullPath.c_str())) != NULL) {
        while ((ent = readdir (dir)) != NULL) {
            // skip ., .., init.mp4
            if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0
                    && strstr(ent->d_name, "init") == NULL) {
                result.push_back(path + ent->d_name);
            }
        }
        closedir (dir);
    } else {
        perror ("cannot open directory");
    }

    return result;
}

/**
 * [parse_to_list_urls description]
 * @param video_folder [description]
 * @param a_node       [description]
 */
// <?xml version="1.0" encoding="UTF-8" standalone="yes"?>
// <MPD >
// <Period>
//      <AdaptationSet mimeType="video/mp4">
//          <Representation>
//              <SegmentTemplate/>
//          </Representation>
//          <Representation>
//               <SegmentTemplate/>
//          </Representation>
//          <Representation>
//               <SegmentTemplate/>
//          </Representation>
//      </AdaptationSet>
// </Period>
static void parse_to_list_urls(const char *video_folder, xmlNode * a_node) {
    xmlNode *cur_node = NULL, *suc_node = NULL, *nxt_suc_node = NULL;
    xmlAttr *curr_attr = NULL;
    xmlChar* attrName = NULL, *attrVal = NULL;

    // iterate through each node at the same level
    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        // try to find <Representation> level
        if (cur_node->type == XML_ELEMENT_NODE && xmlStrcmp(cur_node->name, BAD_CAST REPRESENTATION) == 0) {
            // the next level of <Representation> must be <SegmentTemplate/>
            for (suc_node = cur_node->children; suc_node; ) {
                // try to match <SegmentTemplate/> level
                if (suc_node->type == XML_ELEMENT_NODE && xmlStrcmp(suc_node->name, BAD_CAST SEGMENT_TEMPLATE) == 0) {
                    // put SEGMENT_DURATION, SEGMENT_MEDIA, SEGMENT_TIMESCALE, SEGMENT_INITIALIZATION
                    // into value map
                    for (curr_attr = suc_node->properties; curr_attr; curr_attr = curr_attr->next) {
                        attrName = (xmlChar*)curr_attr->name;
                        attrVal = curr_attr->children->content;

                        if (xmlStrcmp(curr_attr->name, BAD_CAST SEGMENT_DURATION) == 0 || xmlStrcmp(curr_attr->name, BAD_CAST SEGMENT_TIMESCALE) == 0
                                || xmlStrcmp(curr_attr->name, BAD_CAST SEGMENT_MEDIA) == 0 || xmlStrcmp(curr_attr->name, BAD_CAST SEGMENT_INITIALIZATION) == 0) {
                            string key((char*)attrName);
                            string val((char*)attrVal);
                            attrToVal[key] = val;
                        }
                    }

                    nxt_suc_node = suc_node->next;
                    // we need rebuild new xml tree inplace, deletes the SegmentTemplate
                    // replaced with explict nodes
                    xmlUnlinkNode(suc_node);
                    xmlFreeNode(suc_node);
                    suc_node = nxt_suc_node;
                } else {
                    suc_node = suc_node->next;
                }
            }

            // begin to fill new nodes
            size_t found;
            string key, val;

            // <Representation>
            //     <SegmentList SEGMENT_DURATION, SEGMENT_TIMESCALE>
            //     <SegmentList/>
            // <Representation>
            // make a new node SegmentList, fill it with SEGMENT_DURATION, SEGMENT_TIMESCALE
            // add as child of <Representation>
            xmlNodePtr newNodeSegList = xmlNewNode(NULL, BAD_CAST SEGMENT_LIST);
            key = SEGMENT_DURATION;
            val = attrToVal[key];
            xmlNewProp (newNodeSegList, BAD_CAST SEGMENT_DURATION, BAD_CAST val.c_str());
            key = SEGMENT_TIMESCALE;
            val = attrToVal[key];
            xmlNewProp (newNodeSegList, BAD_CAST SEGMENT_TIMESCALE, BAD_CAST val.c_str());
            xmlAddChild(cur_node, newNodeSegList);

            // add initialization to segment base
            // <Representation>
            //     <SegmentList SEGMENT_DURATION, SEGMENT_TIMESCALE>
            //         <Initialization SEGMENT_INITIALIZATION_URL />
            //     <Representation/>
            // <Representation>
            // make a new node Initialization, fill it with SEGMENT_INITIALIZATION_URL
            // add as child of <Representation>
            xmlNodePtr newNodeBaseInit = xmlNewNode(NULL, BAD_CAST SEGMENT_INITIALIZATION_ELEMENT);
            key = SEGMENT_INITIALIZATION;
            val = attrToVal[key];
            xmlNewProp (newNodeBaseInit, BAD_CAST SEGMENT_INITIALIZATION_URL, BAD_CAST val.c_str());
            xmlAddChild(newNodeSegList, newNodeBaseInit);

            // need to find out all the segment chunk path
            // <Representation>
            //     <SegmentList SEGMENT_DURATION, SEGMENT_TIMESCALE>
            //         <Initialization SEGMENT_INITIALIZATION_URL />
            //         <SegmentURL SEGMENT_MEDIA />
            //         <SegmentURL SEGMENT_MEDIA />
            //         ...
            //     <Representation/>
            // <Representation>

            // media="480_1200000/dash/segment_$Number$.m4s"
            key = SEGMENT_MEDIA;
            val = attrToVal[key];
            found = val.find_last_of("/");
            // val: 480_1200000/dash/
            val = val.substr(0, found + 1);
            vector<string> urls = find_all_segment_url(video_folder, val);
            std::sort(urls.begin(), urls.end(), sort_on_segment_id());

            for (int i = 0; i < urls.size(); ++i) {
                xmlNodePtr newNodeSegURL = xmlNewNode(NULL, BAD_CAST SEGMENT_URL);
                key = SEGMENT_MEDIA;
                val = urls[i];
                xmlNewProp (newNodeSegURL, BAD_CAST SEGMENT_MEDIA, BAD_CAST val.c_str());
                xmlAddChild(newNodeSegList, newNodeSegURL);
            }
        }
        // if not <Representation> level, go to next level
        parse_to_list_urls(video_folder, cur_node->children);
    }
}

/**
 * convert file path to dag string
 * @param  video_folder
 * @param  attrVal
 * @param  pathToUrl
 * @return              dag string
 */
static string constructDAGstring(const char* video_folder, xmlChar* attrVal, map<string, vector<string> > & pathToUrl) {
    string fullPath;
    string temp1(video_folder);
    string temp2((char*)attrVal);
    fullPath = temp1 + temp2;

    vector<string> currUrls = pathToUrl[fullPath];

    string dagUrlStr;
    // one file path is mapped to mutiple chunk dags
    for (int i = 0; i < currUrls.size(); ++i) {
        dagUrlStr += currUrls[i] + " ";
    }
    // remove the last space
    dagUrlStr = dagUrlStr.substr(0, dagUrlStr.size() - 1);

    return dagUrlStr;
}

/**
 * parse parsed manifest to dag manifest
 * @param video_folder
 * @param a_node       root node of xml tree
 * @param pathToUrl    map from segment file to different chunks,each chunk has
 *                     dag associated with it
 */
// <Representation>
//     <SegmentList SEGMENT_DURATION, SEGMENT_TIMESCALE>
//         <Initialization SEGMENT_INITIALIZATION_URL />
//         <SegmentURL SEGMENT_MEDIA />
//         <SegmentURL SEGMENT_MEDIA />
//         ...
//     <Representation/>
// <Representation>
static void parse_to_list_dag_urls(const char *video_folder, xmlNode * a_node, map<string, vector<string> > & pathToUrl) {
    xmlNode *cur_node = NULL;
    xmlAttr *curr_attr = NULL;
    xmlChar* attrName = NULL, *attrVal = NULL;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        // it only needs to modify tree content in the node of Initialization and SegmentURL
        if (cur_node->type == XML_ELEMENT_NODE && xmlStrcmp(cur_node->name, BAD_CAST SEGMENT_URL) == 0) {
            // for the case of SegmentURL
            for (curr_attr = cur_node->properties; curr_attr; curr_attr = curr_attr->next) {
                attrName = (xmlChar*)curr_attr->name;
                attrVal = curr_attr->children->content;

                if (xmlStrcmp(attrName, BAD_CAST SEGMENT_MEDIA) == 0) {
                    string dagUrlStr = constructDAGstring(video_folder, attrVal, pathToUrl);
                    // change SEGMENT_MEDIA of SegmentURL to dag urls
                    xmlSetProp(cur_node, BAD_CAST SEGMENT_MEDIA, BAD_CAST dagUrlStr.c_str());
                }
            }
        } else if (cur_node->type == XML_ELEMENT_NODE && xmlStrcmp(cur_node->name, BAD_CAST SEGMENT_INITIALIZATION_ELEMENT) == 0) {
            // for the case of Initialization
            for (curr_attr = cur_node->properties; curr_attr; curr_attr = curr_attr->next) {
                attrName = (xmlChar*)curr_attr->name;
                attrVal = curr_attr->children->content;

                if (xmlStrcmp(attrName, BAD_CAST SEGMENT_INITIALIZATION_URL) == 0) {
                    string dagUrlStr = constructDAGstring(video_folder, attrVal, pathToUrl);
                    // change SEGMENT_INITIALIZATION_URL of SegmentURL to dag urls
                    xmlSetProp(cur_node, BAD_CAST SEGMENT_INITIALIZATION_URL, BAD_CAST dagUrlStr.c_str());
                }
            }
        }
        // or it will go to the next level
        parse_to_list_dag_urls(video_folder, cur_node->children, pathToUrl);
    }
}

/**
 * parse explict chunk manifest file to dag format manifest file
 * @param  video_folder fold that contains video files
 * @param  from_uri     parsed file manifest file path
 * @param  to_uri       generated dag format manifest file path
 * @param  pathToUrl    map from segment file to different chunks,each chunk has
 *                      dag associated with it
 * @return              1 or -1
 */
int generate_XIA_manifest(const char *video_folder, const char *from_uri, const char* to_uri, map<string, vector<string> > & pathToUrl) {
    FILE *fp = fopen(to_uri, "w");

    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;

    xmlInitParser();

    doc = xmlReadFile(from_uri, NULL, 0);
    if (doc == NULL) {
        say("error: could not parse file %s\n", from_uri);
        return -1;
    }

    root_element = xmlDocGetRootElement(doc);
    parse_to_list_dag_urls(video_folder, root_element, pathToUrl);
    xmlDocDump(fp, doc);

    xmlFreeDoc(doc);
    xmlCleanupParser();
    fclose(fp);

    return 1;
}

/**
 * [parse_dash_manifest description]
 * @param  video_folder
 * @param  from_uri     the original mpd template, eg: resources/dash1/155742.mpd
 * @param  to_uri       the parsed mpd output path, eg: resources/dash1/parsed_dash.mpd
 * @return              1 or -1
 */
int parse_dash_manifest(const char *video_folder, const char *from_uri, const char* to_uri) {
    FILE *fp = fopen(to_uri, "w");

    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;

    // Initialization of the XML parser
    xmlInitParser();

    // forms a XML tree from file 'from_uri', NULL is the default encoding
    // 0 is the options
    doc = xmlReadFile(from_uri, NULL, 0);
    if (doc == NULL) {
        say("error: could not parse file %s\n", from_uri);
        return -1;
    }

    root_element = xmlDocGetRootElement(doc);
    parse_to_list_urls(video_folder, root_element);
    // write the new xml tree back to file, and save it
    xmlDocDump(fp, doc);

    xmlFreeDoc(doc);
    xmlCleanupParser();
    fclose(fp);

    return 1;
}


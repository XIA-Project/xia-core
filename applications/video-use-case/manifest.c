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

struct sort_on_segment_id{
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

static vector<string> find_all_segment_url(const char *video_folder, string & path){
    vector<string> result;
    string fullPath = video_folder + path;

    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir (fullPath.c_str())) != NULL) {
        while ((ent = readdir (dir)) != NULL) {
            if(strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0
                    && strstr(ent->d_name, "init") == NULL){
                result.push_back(path + ent->d_name);
            }
        }
        closedir (dir);
    } else {
        perror ("cannot open directory");
    }

    return result;
}

static void parse_to_list_urls(const char *video_folder, xmlNode * a_node) {
    xmlNode *cur_node = NULL, *suc_node = NULL, *nxt_suc_node = NULL;
    xmlAttr *curr_attr = NULL;
    xmlChar* attrName = NULL, *attrVal = NULL;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE && xmlStrcmp(cur_node->name, BAD_CAST REPRESENTATION) == 0) {

            for (suc_node = cur_node->children; suc_node; ){

                if (suc_node->type == XML_ELEMENT_NODE && xmlStrcmp(suc_node->name, BAD_CAST SEGMENT_TEMPLATE) == 0) {

                    for (curr_attr = suc_node->properties; curr_attr; curr_attr = curr_attr->next){
                        attrName = (xmlChar*)curr_attr->name;
                        attrVal = curr_attr->children->content;

                        if (xmlStrcmp(curr_attr->name, BAD_CAST SEGMENT_DURATION) == 0 || xmlStrcmp(curr_attr->name, BAD_CAST SEGMENT_TIMESCALE) == 0
                                    || xmlStrcmp(curr_attr->name, BAD_CAST SEGMENT_MEDIA) == 0 || xmlStrcmp(curr_attr->name, BAD_CAST SEGMENT_INITIALIZATION) == 0){
                            string key((char*)attrName);
                            string val((char*)attrVal);
                            attrToVal[key] = val;
                        }
                    }

                    nxt_suc_node = suc_node->next;
                    xmlUnlinkNode(suc_node);
                    xmlFreeNode(suc_node);
                    suc_node = nxt_suc_node;
                } else {
                    suc_node = suc_node->next;
                }
            }

            size_t found;
            string key, val;

            xmlNodePtr newNodeSegList = xmlNewNode(NULL, BAD_CAST SEGMENT_LIST);
            key = SEGMENT_DURATION;
            val = attrToVal[key];
            xmlNewProp (newNodeSegList, BAD_CAST SEGMENT_DURATION, BAD_CAST val.c_str());
            key = SEGMENT_TIMESCALE;
            val = attrToVal[key];
            xmlNewProp (newNodeSegList, BAD_CAST SEGMENT_TIMESCALE, BAD_CAST val.c_str());
            xmlAddChild(cur_node, newNodeSegList);

             // add initialization to segment base
            xmlNodePtr newNodeBaseInit = xmlNewNode(NULL, BAD_CAST SEGMENT_INITIALIZATION_ELEMENT);
            key = SEGMENT_INITIALIZATION;
            val = attrToVal[key];
            xmlNewProp (newNodeBaseInit, BAD_CAST SEGMENT_INITIALIZATION_URL, BAD_CAST val.c_str());
            xmlAddChild(newNodeSegList, newNodeBaseInit);

            // need to find out all the segment chunk path
            key = SEGMENT_MEDIA;
            val = attrToVal[key];
            found = val.find_last_of("/");
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

        parse_to_list_urls(video_folder, cur_node->children);
    }
}

static string constructDAGstring(const char* video_folder, xmlChar* attrVal, map<string, string>& pathToUrl)
{
    string fullPath(video_folder);

	fullPath += string((char *)attrVal);
    return pathToUrl[fullPath];
}

static void parse_to_list_dag_urls(const char *video_folder, xmlNode * a_node, map<string, string> &pathToUrl) {
    xmlNode *cur_node = NULL;
    xmlAttr *curr_attr = NULL;
    xmlChar* attrName = NULL, *attrVal = NULL;
	static string bandwidth;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {

        if (cur_node->type == XML_ELEMENT_NODE && xmlStrcmp(cur_node->name, BAD_CAST REPRESENTATION) == 0) {
            for (curr_attr = cur_node->properties; curr_attr; curr_attr = curr_attr->next){
                attrName = (xmlChar*)curr_attr->name;
                attrVal = curr_attr->children->content;

                if (xmlStrcmp(attrName, BAD_CAST REPRESENTATION_BANDWIDTH) == 0){
					bandwidth = (const char *) attrVal;
                }
            }
         } else if (cur_node->type == XML_ELEMENT_NODE && xmlStrcmp(cur_node->name, BAD_CAST SEGMENT_URL) == 0) {

            for (curr_attr = cur_node->properties; curr_attr; curr_attr = curr_attr->next){
                attrName = (xmlChar*)curr_attr->name;
                attrVal = curr_attr->children->content;

                if (xmlStrcmp(attrName, BAD_CAST SEGMENT_MEDIA) == 0){
                    string dagUrlStr = constructDAGstring(video_folder, attrVal, pathToUrl);

					dagUrlStr += "?bandwidth=" + bandwidth;
                    xmlSetProp(cur_node, BAD_CAST SEGMENT_MEDIA, BAD_CAST dagUrlStr.c_str());
                }
            }
        } else if (cur_node->type == XML_ELEMENT_NODE && xmlStrcmp(cur_node->name, BAD_CAST SEGMENT_INITIALIZATION_ELEMENT) == 0){

            for (curr_attr = cur_node->properties; curr_attr; curr_attr = curr_attr->next){
                attrName = (xmlChar*)curr_attr->name;
                attrVal = curr_attr->children->content;

                if (xmlStrcmp(attrName, BAD_CAST SEGMENT_INITIALIZATION_URL) == 0){
                    string dagUrlStr = constructDAGstring(video_folder, attrVal, pathToUrl);
					dagUrlStr += "?bandwidth=" + bandwidth;
                    xmlSetProp(cur_node, BAD_CAST SEGMENT_INITIALIZATION_URL, BAD_CAST dagUrlStr.c_str());
                }
            }
        }

        parse_to_list_dag_urls(video_folder, cur_node->children, pathToUrl);
    }
}


int generate_XIA_manifest(const char *video_folder, const char *from_uri, const char* to_uri, map<string, string> & pathToUrl){
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

int parse_dash_manifest(const char *video_folder, const char *from_uri, const char* to_uri){
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
    parse_to_list_urls(video_folder, root_element);
    xmlDocDump(fp, doc);

    xmlFreeDoc(doc);
    xmlCleanupParser();
    fclose(fp);

    return 1;
}


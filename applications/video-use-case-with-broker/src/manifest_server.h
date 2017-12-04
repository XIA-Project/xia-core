#ifndef __VIDEO_SERVER_H__
#define __VIDEO_SERVER_H__

#include <vector>
#include "Xsocket.h"
#include "Xkeys.h"
#include "dagaddr.hpp"
#include "utils.h"

#define VERSION "v1.0"
#define TITLE "XIA Video Manifest Server"

#define VIDEO_ORIGIN_SERVER_NAME "www.origin.xia"

using namespace std;

/**
 * helper functions
 */
void help(const char *name);
void get_config(int argc, char** argv);

/**
 * process request for each new video request
 * basically return back the manifest url back 
 * to the proxy
 * 
 * @param socketid XIA proxy socket
 */
void* process_request(void* socketid);

/**
 * register the server receiver
 * @return server XIA socket
 */
int register_receiver();

/**
 * for a given request from proxy, read the manifest url
 * locally which then is passed to proxy
 *
 * proxy then request the actual manifest from the 
 * video server
 * 
 * @param  proxy_sock proxy socket
 * @param  message    request message
 * @return status > 0 ok, < 0 false
 */
int handle_xhttp_request(int proxy_sock, char* message);

#endif 
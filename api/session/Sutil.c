/*
** Copyright 2011 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
/*!
  @file Sutil.c
  @brief Impliments internal socket helper functions
*/
#include "session.h"
#include "Sutil.h"
#include "api_init.h"
#include <errno.h>
#include <sstream>
#include <vector>

#define BUFSIZE 65000


int proc_send(int sockfd, session::SessionMsg *sm)
{
	int rc = 0;
	struct sockaddr_in sa;

	assert(sm);

	// TODO: cache these so we don't have to set everything up each time we
	// are called
	sa.sin_family = PF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
    sa.sin_port = htons(PROCPORT);

	std::string p_buf;
	sm->SerializeToString(&p_buf);

	int remaining = p_buf.size();
	const char *p = p_buf.c_str();
	while (remaining > 0) {
		rc = sendto(sockfd, p, remaining, 0, (struct sockaddr *)&sa, sizeof(sa));

		if (rc == -1) {
			ERRORF("click socket failure: errno = %d", errno);
			break;
		} else {
			remaining -= rc;
			p += rc;
			if (remaining > 0) {
				DBGF("%d bytes left to send", remaining);
			}
		}	
	}

	return  (rc >= 0 ? 0 : -1);
}

int proc_reply(int sockfd, session::SessionMsg &sm)
{
	struct sockaddr_in sa;
	socklen_t len;
	int rc;
	char buf[BUFSIZE];
	unsigned buflen = sizeof(buf);

	len = sizeof sa;

	memset(buf, 0, buflen);
	if ((rc = recvfrom(sockfd, buf, buflen - 1 , 0, (struct sockaddr *)&sa, &len)) < 0) {
		ERRORF("error(%d) getting reply data from session process", errno);
		return -1;
	}
	
	std::string p_buf(buf, rc);  // make a std string to deal with null chars
	sm.ParseFromString(p_buf);

	return rc;
}




std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::string trim(const std::string& str)
{
	const std::string& whitespace = " \t\n";
    const size_t strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos)
        return ""; // no content

    const size_t strEnd = str.find_last_not_of(whitespace);
    const size_t strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
}

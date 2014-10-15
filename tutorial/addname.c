/* ts=4 */
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

/* simple app to add a name to the name server */

#include "Xsocket.h"
#include "dagaddr.hpp"


int main(int argc, char **argv)
{
    if (argc != 3) {
	printf("usage: addname name dag\n");
	exit(1);
    }

    char *name = argv[1];
    char *dag = argv[2];
    sockaddr_x sa;
    Graph g(dag);

    // simple evalutation of correctness
    if (g.num_nodes() == 0) {
	printf("error: DAG is empty or malformed\n");
	exit(2);
    }
    g.fill_sockaddr(&sa);

    //Register this service name to the name server
    if (XregisterName(name, &sa) < 0) {
    	printf("error: unable to register name/dag combo");
	exit(1);
    }

    printf("%s added to nameserver\n", name);
    return 0;
}


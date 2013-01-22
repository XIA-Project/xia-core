#
# Copyright 2013 Carnegie Mellon University
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import sys
import os
                                                                                                                                                                                                                    
# find the path to xia-core
XIADIR=os.getcwd()
while os.path.split(XIADIR)[1] != 'xia-core':
    XIADIR=os.path.split(XIADIR)[0]
sys.path.append(XIADIR + '/api/lib')

from dagaddr import *

def main():

    n_src = Node()
    n_ad = Node(Node.XID_TYPE_AD, "0606060606060606060606060606060606060606")
    n_hid = Node(Node.XID_TYPE_HID, "0101010101010101010101010101010101010101")
    n_cid = Node(Node.XID_TYPE_CID, "0202020202020202020202020202020202020202")

    # Path directly to n_cid
    # n_src -> n_cid
    print 'g0 = n_src * n_cid'
    g0 = n_src * n_cid
    g0.print_graph()
    print ''

    # Path to n_cid through n_hid
    # n_src -> n_hid -> n_cid
    print 'g1 = n_src * n_hid * n_cid'
    g1 = n_src * n_hid * n_cid
    g1.print_graph()
    print ''

    # Path to n_cid through n_ad then n_hid
    # n_src -> n_ad -> n_hid -> n_cid
    print 'g2 = n_src * n_ad * n_hid * n_cid'
    g2 = n_src * n_ad * n_hid * n_cid
    g2.print_graph()
    print ''

    # Combine the above three paths into a single DAG;
    # g1 and g2 become fallback paths from n_src to n_cid
    print 'g3 = g0 + g1 + g2'
    g3 = g0 + g1 + g2
    g3.print_graph()
    print ''

    # Get a DAG string version of the graph that could be used in an
    # XSocket API call
    dag_string = g3.dag_string()
    print dag_string
    
    # Create a DAG from a string (which we might have gotten from an Xsocket
    # API call like XrecvFrom)
    g4 = Graph(dag_string)
    g4.print_graph()
    print ''


    # TODO: Cut here for example version

    print '\n'
    print 'g5 = g3 * (SID0 + SID1) * SID2'
    g5 = g3 * (Node(Node.XID_TYPE_SID, "0303030303030303030303030303030303030303") + Node(Node.XID_TYPE_SID, "0404040404040404040404040404040404040404")) * Node(Node.XID_TYPE_SID, "0505050505050505050505050505050505050505")
    g5.print_graph()
    print ''
    print '%s\n' % g5.dag_string()

    print 'g5_prime = Graph(g5.dag_string())'
    g5_prime = Graph(g5.dag_string())
    g5_prime.print_graph()
    print '\n'


if __name__ == "__main__":
    main()

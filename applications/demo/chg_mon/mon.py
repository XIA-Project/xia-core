#!/usr/bin/env python
import sys
import zmq
import reporting_pb2
import os
import networkx as nx

import matplotlib
import matplotlib.pyplot as plt
import matplotlib.colors

# find the path to xia-core
XIADIR=os.getcwd()
while os.path.split(XIADIR)[1] != 'xia-core':
    XIADIR=os.path.split(XIADIR)[0]
sys.path.append(XIADIR + '/api/lib')

import dagaddr


SERVERS=["localhost","127.0.0.1"]
PORT = "1234"

class QuickNode:
    def __init__(self, idx, label, dag):
        self.idx = int(idx)
        self.dag = dag
        self.label = label
        self.sink = False
        
        if (self.idx) == 0:
            assert (self.label == "DAG")
            self.ntype = "Root"
            self.xid = None
            
        else:
            self.ntype, self.xid = self.label.split(":")


    def successors(self):
        return self.dag.__out_edges(self.idx)

    def _set_sink(self):
        self.sink = True

    def __repr__(self):
        sinkstr = ""
        if self.sink:
            sinkstr = " (SINK)"
        return "<" + str(self.idx) + ": " + self.ntype + " " + str(self.xid) + sinkstr + ">"
    
    def getLabel(self, maxchars=4):
        if self.ntype == 'Root':
            return ""
        else:
            return str(self.ntype) + "\n" + str(self.xid)[0:maxchars-1]


class QuickDag:
    def __init__(self, dagstr):
        lines = dagstr.split('-')
        lines = [l.strip() for l in lines]
        foo = [l.split(None, 1) for l in lines]
        self.nodes = []
        self.edges = []
        for idx,entry in enumerate(foo):
            ##print entry
            n = QuickNode(idx, entry[0], self)
            try:
                for edge_idx, next_node in enumerate(entry[1]):
                    ##print next_node
                    self.edges.append((idx, int(next_node)+1, edge_idx))
            except IndexError:
                n._set_sink()
            self.nodes.append(n)

    def __str__(self):
        return "Nodes: " + str(self.nodes) + ", Edges: " + str(self.edges)

        
    def __out_edges(self, src_idx):
        return [(src, dst, pri) for e in self.edges if src == src_idx]


    def root(self):
        return self.nodes[0]

    def toNxDiGraph(self):
        dg = nx.DiGraph()
        dg.add_nodes_from(self.nodes)
        for (src_idx, dst_idx, edge_idx) in self.edges:
            ##print self.nodes[src_idx], dst_idx
            dg.add_edge(self.nodes[src_idx],
                        self.nodes[dst_idx],
                        priority=edge_idx
            )
        #print dg.nodes()
        #print dg.edges()
        pos = {}
        pos[self.root()] = (0,0)
        for (src,dst) in nx.dfs_edges(dg, self.root()):
            edge_pri = dg[src][dst]['priority']
            if dst not in pos:
                (src_x, src_y) = pos[src]
                dst_x = src_x + 10
                dst_y = src_y + (edge_pri * 10)
                pos[dst] = (dst_x, dst_y)
        
        labels = {}
        allcolors = matplotlib.colors.cnames.keys()
        colorlist = []

        for node in self.nodes:
            labels[node] = node.getLabel()
            colorlist.append(allcolors[(hash(node.getLabel()) % len(allcolors))])
        
        return (dg, pos, labels, colorlist)

def watch(socket, debug=True):
    
    pb_msg = reporting_pb2.AddrChange()
    
    while True:
        header,msg = socket.recv_multipart()
        if debug:
            sys.stderr.write("Got change message: %s\n" % header)
        pb_msg.ParseFromString(msg)
        if debug:
            s = str(pb_msg)
            new_s = '\n'.join(['\t' + l for l in s.split('\n')])
            sys.stderr.write(new_s+'\n')
        ##print(pb_msg)
        ##print pb_msg.newdag
        showdag (str(pb_msg.newdag), str(pb_msg.intent), str(pb_msg.whoami))
        
    return 0
    


def tests():
    str1 = "RE AD:3d6a88606ef8d04ec4b8a5983a570083bace5690 HID:91b536b0c86a91c15cd614ba4adf132387774df4 SID:785fb7e281640ad1a4fc6d6349eac538be395344"
    str2 = "RE AD:3d6a88606ef8d04ec4b8a5983a570083bace5690 HID:01b536b0c86a91c15cd614ba4adf132387774df4 SID:785fb7e281640ad1a4fc6d6349eac538be395344"

    for str in [str1, str2]:
        g = dagaddr.Graph(str1)
        print(g)
        str2 = g.dag_string()
        g.print_graph()
        print(str2)
        q = QuickDag(str2)
        print(q)
        dg, pos, labels, colors = q.toNxDiGraph()
        print dg.number_of_edges()
        nx.draw(dg, pos, with_labels=False, node_size=1500, node_color='w', alpha=0.5)
        nx.draw_networkx_labels(dg, pos, labels)
        plt.show()
    return 

def showdag(str1, intent, whoami, debug=False):
    fig = plt.figure()
    ## Normalize to DAG text format (not RE format)
    str1 = dagaddr.Graph(str1).dag_string()
    if debug:
        sys.stderr.write("Processed DAG to %s\n" % str1)
    q = QuickDag(str1)
    if debug:
        sys.stderr.write("Generated QuickDag: %s\n" % str(q))
    dg, pos, labels, colors = q.toNxDiGraph()
    nx.draw(dg, pos, with_labels=False, node_size=1500, node_color='w', alpha=0.5,
            label="DAG for %s as seen by %s" % (intent, whoami))
    nx.draw_networkx_labels(dg, pos, labels)
    plt.savefig("%s_from_%s.png"%(intent,whoami))
    #return fig
    

def main(args):

    ## Init ZMQ
    context = zmq.Context()
    socket = context.socket(zmq.SUB)

    for s in SERVERS:
        socket.connect("tcp://%s:%s" % (s, PORT))
        sys.stderr.write("Connecting to tcp://%s:%s\n" % (s,PORT))
    socket.setsockopt(zmq.SUBSCRIBE, "/dagchange/")
    sys.stderr.write("listening...\n")
    watch(socket)
    #tests()


if __name__ == '__main__':
    sys.exit(main(sys.argv))

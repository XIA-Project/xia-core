#!/usr/bin/env python
import sys
import zmq
import reporting_pb2
import os
import pygraphviz
import networkx as nx
from threading import Thread
from time import sleep

import wx
import matplotlib
matplotlib.use('WXAgg')
import matplotlib.pyplot as plt

from traitsui.wx.editor import Editor
from traits.api import *
from traitsui.api import View, Item
from traitsui.wx.basic_editor_factory import BasicEditorFactory
from matplotlib.backends.backend_wxagg import FigureCanvasWxAgg as FigureCanvas
from matplotlib.backends.backend_wx import NavigationToolbar2Wx


# find the path to xia-core
XIADIR=os.getcwd()
while os.path.split(XIADIR)[1] != 'xia-core':
    XIADIR=os.path.split(XIADIR)[0]
sys.path.append(XIADIR + '/api/lib')

import dagaddr


SERVER="localhost"
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
            print entry
            n = QuickNode(idx, entry[0], self)
            try:
                for edge_idx, next_node in enumerate(entry[1]):
                    print next_node
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
            print self.nodes[src_idx], dst_idx
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
        for node in self.nodes:
            labels[node] = node.getLabel()
        
        return (dg, pos, labels)

def tests():
    str1 = "RE AD:3d6a88606ef8d04ec4b8a5983a570083bace5690 HID:91b536b0c86a91c15cd614ba4adf132387774df4 SID:785fb7e281640ad1a4fc6d6349eac538be395344"
    g = dagaddr.Graph(str1)
    print(g)
    str2 = g.dag_string()
    g.print_graph()
    print(str2)
    q = QuickDag(str2)
    print(q)
    dg, pos, labels = q.toNxDiGraph()
    print dg.number_of_edges()
    nx.draw(dg, pos, with_labels=False, node_size=1500, node_color='w')
    nx.draw_networkx_labels(dg, pos, labels)
    plt.show()
    return 

def showdag(str1, intent, whoami, debug=True):
    fig = plt.figure()
    ## Normalize to DAG text format (not RE format)
    str1 = dagaddr.Graph(str1).dag_string()
    if debug:
        sys.stderr.write("Processed DAG to %s\n" % str1)
    q = QuickDag(str1)
    if debug:
        sys.stderr.write("Generated QuickDag: %s\n" % str(q))
    dg, pos, labels = q.toNxDiGraph()
    nx.draw(dg, pos, with_labels=False, node_size=1500, node_color='w',
            label="DAG for %s as seen by %s" % (intent, whoami))
    nx.draw_networkx_labels(dg, pos, labels)
    plt.savefig("%s_from_%s.png"%(intent,whoami))
    #return fig
    
class _MPLFigureEditor(Editor):
    scrollable = True
    
    def init(self, parent):
        self.control = self._create_canvas(parent)
        self.set_tooltip()
    
    def update_editor(self):
        pass
    
    def _create_canvas(self, parent):
        """ Create the MPL canvas."""
        panel= wx.Panel(parent, -1, style=wx.CLIP_CHILDREN)
        sizer = wx.BoxSizer(wx.VERTICAL)
        panel.SetSizer(sizer)

        mpl_control = FigureCanvas(panel, -1, self.value)
        sizer.Add(mpl_control, 1, wx.LEFT | wx.TOP | wx.GROW)
        toolbar = NavigationToolbar2Wx(mpl_control)
        sizer.Add(toolbar, 0, wx.EXPAND)
        self.value.canvas.SetMinSize((10,10))
        return panel


class MPLFigureEditor(BasicEditorFactory):
    klass = _MPLFigureEditor

def watch(socket):
    socket.connect("tcp://%s:%s" % (SERVER, PORT))
    socket.setsockopt(zmq.SUBSCRIBE, "/dagchange/")
    
    pb_msg = reporting_pb2.AddrChange()
    
    while True:
        header,msg = socket.recv_multipart()
        print(header)
        pb_msg.ParseFromString(msg)
        print(pb_msg)
        print pb_msg.newdag
        showdag (str(pb_msg.newdag), str(pb_msg.intent), str(pb_msg.whoami))
        
    return 0
    

class SocketWatcherThread(Thread):
    """Listens for ZMQ/Protobuf messages"""
    wants_abort = False

    def run(self):
        """Wait and do things"""
        self.socket.connect("tcp://%s:%s" % (SERVER, PORT))
        self.socket.setsockopt(zmq.SUBSCRIBE, "/dagchange/")
        pb_msg = reporting_pb2.AddrChange()

        print "running"
        while not self.wants_abort:
            header,msg = self.socket.recv_multipart()
            print(header)
            pb_msg.ParseFromString(msg)
            print(pb_msg)
            self.display(str(pb_msg.newdag))
            self.dag_show(str(pb_msg.newdag))

class Test (HasTraits):
    figure = Instance(matplotlib.figure.Figure, ())
    socket_watcher_thread = Instance(SocketWatcherThread)
    results_string = String()

    view = View(Item('figure', editor=MPLFigureEditor(), show_label=False),
                Item('results_string', show_label = False, springy=True),
            width=400,
                height=300,
                resizable=True)
    
        
    def _figure_default(self):
        figure = matplotlib.figure.Figure()
        return figure
            
    def __init__(self):
        super(Test, self).__init__()
        context = zmq.Context()
        socket = context.socket(zmq.SUB)

        axes = self.figure.add_subplot(111)
            
        #Start socket watcher
        if self.socket_watcher_thread and self.socket_watcher_thread.isAlive():
            pass
        else:
            self.socket_watcher_thread = SocketWatcherThread()
            self.socket_watcher_thread.socket = socket
            self.socket_watcher_thread.display = self.add_line
            self.socket_watcher_thread.dag_show = self.dag_show
            self.socket_watcher_thread.ax = axes
            self.socket_watcher_thread.start()
                
        def add_line(self, string):
            self.results_string = self.results_string + "\n" + string

        def dag_show(self, dagstr):
            showdag(dagstr, self.figure.axes)
            wx.CallAfter(self.figure.canvas.draw)
            return


def main(args):
    ## Init ZMQ
    context = zmq.Context()
    socket = context.socket(zmq.SUB)

    watch(socket)
    #tests()


if __name__ == '__main__':
    sys.exit(main(sys.argv))

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




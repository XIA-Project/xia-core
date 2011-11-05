from PyQt4.QtGui import QDialog
from PyQt4.QtCore import QTimer
from ui_main import Ui_Main
import os
import time
import fcntl
import subprocess

# for plotting
import matplotlib
from matplotlib.backends.backend_qt4agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.backends.backend_qt4agg import NavigationToolbar2QTAgg as NavigationToolbar
from matplotlib.figure import Figure


class Main(QDialog, Ui_Main):
    def __init__(self):
        QDialog.__init__(self)

        self.setupUi(self)

        for name, value in self.__dict__.items():
            if name.startswith('radioButton_FB_'):
                value.clicked.connect(self.set_FB)

        for name, value in self.__dict__.items():
            if name.startswith('radioButton_PS_'):
                value.clicked.connect(self.reset_tgen)

        self.pushButton_Router.clicked.connect(self.reset_router)
        self.pushButton_Monitor.clicked.connect(self.reset_monitor)
        self.pushButton_TGen.clicked.connect(self.reset_tgen)

        self.timer = QTimer()
        self.timer.timeout.connect(self.timer_timeout)
        self.timer.start(100)

        self.monitor_p = None
        self.monitor_output_buf = ''

        dpi = 70.
        self.fig = Figure((self.frame_plot.width() / dpi, self.frame_plot.height() / dpi), dpi=dpi)
        self.canvas = FigureCanvas(self.fig)
        self.canvas.setParent(self.frame_plot)

        self.axes = self.fig.add_subplot(111)
        #self.mpl_toolbar = NavigationToolbar(self.canvas, self.frame_plot)

        self.times = []
        self.pps = []
        self.ps = 0

        self.reset_monitor()
        self.reset_tgen()

    def on_draw(self):
        self.axes.clear()

        self.axes.grid(True)
        self.axes.set_xlabel('Elapsed Time')
        self.axes.set_ylabel('Forwarding Speed (Gbps)')

        if self.times:
            xs = map(lambda t: t - self.times[0], self.times)
            gbps = map(lambda pps: pps * self.ps * 8. / 1000000000, self.pps)
            self.axes.plot(xs, gbps)

            duration = 120
            if len(self.times) >= duration:
                del self.times[:-duration]
                del self.pps[:-duration]

        self.canvas.draw()


    def enum_controls(self):
        def extract_key(x):
            try:
                return int(x[0].rpartition('_')[2])
            except:
                return x[0]
        return sorted(self.__dict__.items(), key=extract_key)

    def set_FB(self):
        sel_fb = None

        for name, value in self.enum_controls():
            if name.startswith('radioButton_FB_'):
                if value.isChecked():
                    sel_fb = int(name.rpartition('_')[2])
                    break

        choose_larger_PS = False
        for name, value in self.enum_controls():
            if name.startswith('radioButton_PS_'):
                ps = int(name.rpartition('_')[2])
                enabled = self.calc_payload_size(sel_fb, ps) >= 0
                value.setEnabled(enabled)
                if not enabled:
                    if value.isChecked():
                        # current packet size is too small
                        choose_larger_PS = True
                        value.setChecked(False)
                else:
                    if choose_larger_PS:
                        value.setChecked(True)
                        choose_larger_PS = False

        self.reset_tgen()

    def calc_payload_size(self, fb, ps):
        return ps - (14 + 20 + 8 + 28 * fb + 28)   # Ethernet + IP + XIA_fixed + XIA_dest_addr + XIA_src_addr

    def timer_timeout(self):
        if self.monitor_p:
            try:
                self.monitor_output_buf += os.read(self.monitor_fd, 1024)
            except OSError:
                # probably EWOULDBLOCK or so
                pass

        if '\n' in self.monitor_output_buf:
            line, _, self.monitor_output_buf = self.monitor_output_buf.partition('\n')
            print line
            if line.startswith('TX pps '):
                pps = float(line.rpartition(' ')[2])
                self.times.append(time.time())
                self.pps.append(pps)
                self.on_draw()

    def reset_router(self):
        os.system('ssh -f xia-router0 "killall click; xia-core/click-2.0/conf/xia/script/run_xia_router.sh"')

    def reset_monitor(self):
        if self.monitor_p:
            self.monitor_p.terminate()
        self.monitor_p = subprocess.Popen('ssh xia-router0 "killall ruby; xia-core/click-2.0/conf/interface_stat_all.rb 100000"', stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=True)
        self.monitor_fd = self.monitor_p.stdout.fileno()

        fl = fcntl.fcntl(self.monitor_fd, fcntl.F_GETFL)
        fcntl.fcntl(self.monitor_fd, fcntl.F_SETFL, fl | os.O_NONBLOCK)

    def reset_tgen(self):
        sel_fb = None
        sel_ps = None

        for name, value in self.enum_controls():
            if name.startswith('radioButton_FB_'):
                if value.isChecked():
                    sel_fb = int(name.rpartition('_')[2])
                    break

        for name, value in self.enum_controls():
            if name.startswith('radioButton_PS_'):
                if value.isChecked():
                    sel_ps = int(name.rpartition('_')[2])
                    break

        self.ps = sel_ps

        payload_size = self.calc_payload_size(sel_fb, sel_ps)
        assert payload_size >= 0

        os.system('ssh -f xia-router1 "killall click; xia-core/click-2.0/conf/xia/script/run_xia_pktgen_anyfb.py %d %d"' % (sel_fb, payload_size))


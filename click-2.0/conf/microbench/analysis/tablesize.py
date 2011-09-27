#!/usr/bin/env python

from common import *

family_names = ('HID',)
labels = {'HID': 'HID forwarding', 'AD': 'AD forwarding'}
markers = {'HID': 'x', 'AD': 'o'}
linestyles = {'HID': '-', 'AD': '-'}
x_values = {
    #'HID': (10000, 30000, 100000, 300000, 1000000, 3000000, 10000000, 30000000),
    'HID': (10000, 30000, 100000, 300000, 1000000, 3000000, 10000000),
    'AD': (351611, ),
}
#x_ticks = (10000, 30000, 100000, 300000, 1000000, 3000000, 10000000, 30000000)
#x_ticklabels = ('10 K', '30 K', '100 K', '300 K', '1 M', '3 M', '10 M', '30 M')
x_ticks = (10000, 30000, 100000, 300000, 1000000, 3000000, 10000000)
x_ticklabels = ('10 K', '30 K', '100 K', '300 K', '1 M', '3 M', '10 M')
y_values = {}
y_lower_err_values = {}
y_upper_err_values = {}
for family_name in family_names:
    y_values[family_name] = []
    y_lower_err_values[family_name] = []
    y_upper_err_values[family_name] = []
    for x_value in x_values[family_name]:
        pps_min = None
        pps_max = None
        pps_avg = 0
        for iter_i in range(0, iter_max):
            if family_name != 'AD':
                data_name = 'TABLESIZE_' + family_name + '_%d' % x_value
            else:
                data_name = 'FB0'
            #t = get_processing_time(dataset[data_name] + '_timing' + '_%d' % iter_i)
            #pps = packet / t
            pps = get_pps(dataset[data_name] + '_timing' + '_%d' % iter_i)
            if pps_min is None or pps_min > pps:
                pps_min = pps
            if pps_max is None or pps_max < pps:
                pps_max = pps
            pps_avg += pps / iter_max
            
        y_values[family_name].append(pps_avg / 1000000) # Mpkt/sec
        y_lower_err_values[family_name].append((pps_avg - pps_min) / 1000000)
        y_upper_err_values[family_name].append((pps_max - pps_avg) / 1000000)
        print '%s x=%f y=%f' % (family_name, x_value, y_values[family_name][-1])

fig = plt.figure(figsize=(7, 7 * 0.6))
ax = fig.add_subplot(111)

for family_name in family_names:
    #ax.plot(x_values[family_name], y_values[family_name], label=family_name, marker=markers[family_name], linestyle=linestyles[family_name], color='0')
    ax.semilogx(x_values[family_name], y_values[family_name], label=labels[family_name], marker=markers[family_name], linestyle=linestyles[family_name], color='0')
    yerr = (y_lower_err_values[family_name], y_upper_err_values[family_name])
    ax.errorbar(x_values[family_name], y_values[family_name], yerr=yerr, linestyle=linestyles[family_name], color='0')

ax.set_xlabel('HID forwarding table size (number of entries)')
ax.set_ylabel('Packet processing throughput (Mpkt/sec)')
ax.set_xticks(x_ticks)
ax.set_xticklabels(x_ticklabels)
ax.set_xlim(xmin=6000, xmax=48000000)
ax.set_ylim(ymin=0, ymax=2.5)
ax.set_yticks((0.0, 0.5, 1.0, 1.5, 2.0, 2.5))
ax.yaxis.grid()
#ax.grid()
#ax.legend(loc='lower left').draw_frame(False)

cache_size = 2 * 6000 * 1000
#min_rt_entry_size = (4 + 20) + 8 + 8        # (XID type, ID), port, chain pointer
min_rt_entry_size = 64                      # adds more bytes to take into account low load factor
vline_x = 1. * cache_size / min_rt_entry_size
print 'cache limit: %f' % vline_x
ax.axvline(x=vline_x, linestyle=':', color='0');
ax.annotate('Running out of L2 cache', xy=(vline_x, 0.75), textcoords='offset points',
            xytext=(35, -6), arrowprops=dict(arrowstyle="<-"))

plt.savefig('tablesize.pdf', format='pdf', bbox_inches='tight')


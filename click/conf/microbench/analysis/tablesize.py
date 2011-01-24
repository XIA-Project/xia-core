#!/usr/bin/env python

from common import *

family_names = ('CID', 'AD')
markers = {'CID': 'o', 'AD': 'x'}
linestyles = {'CID': '-', 'AD': '--'}
x_values = (10000, 30000, 100000, 300000, 1000000, 3000000, 10000000, 30000000)
x_ticks = (10000, 30000, 100000, 300000, 1000000, 3000000, 10000000, 30000000)
x_ticklabels = ('10 K', '30 K', '100 K', '300 K', '1 M', '3 M', '10 M', '30 M')
y_values = {}
y_lower_err_values = {}
y_upper_err_values = {}
for family_name in family_names:
    y_values[family_name] = []
    y_lower_err_values[family_name] = []
    y_upper_err_values[family_name] = []
    for x_value in x_values:
        pps_min = None
        pps_max = None
        pps_avg = 0
        for iter_i in range(0, iter_max):
            t = get_processing_time(dataset['TABLESIZE_' + family_name + '_%d' % x_value] + '_timing' + '_%d' % iter_i)
            pps = packet / t
            if pps_min is None or pps_min > pps:
                pps_min = pps
            if pps_max is None or pps_max < pps:
                pps_max = pps
            pps_avg += pps / iter_max
            
        y_values[family_name].append(pps_avg / 1000000) # Mpkt/sec
        y_lower_err_values[family_name].append((pps_avg - pps_min) / 1000000)
        y_upper_err_values[family_name].append((pps_max - pps_avg) / 1000000)

fig = plt.figure(figsize=(7, 7 * 0.6))
ax = fig.add_subplot(111)

for family_name in family_names:
    #ax.plot(x_values, y_values[family_name], label=family_name, marker=markers[family_name], linestyle=linestyles[family_name], color='0')
    ax.semilogx(x_values, y_values[family_name], label=family_name, marker=markers[family_name], linestyle=linestyles[family_name], color='0')
    yerr = (y_lower_err_values[family_name], y_upper_err_values[family_name])
    ax.errorbar(x_values, y_values[family_name], yerr=yerr, linestyle=linestyles[family_name], color='0')

ax.set_xlabel('CID routing table size (number of entries)')
ax.set_ylabel('Packet processing throughput (Mpkt/sec)')
ax.set_xticks(x_ticks)
ax.set_xticklabels(x_ticklabels)
ax.set_xlim(xmin=6000, xmax=48000000)
ax.set_ylim(ymin=1.1, ymax=2.1)
ax.set_yticks((1.2, 1.4, 1.6, 1.8, 2.0))
ax.grid()
ax.legend(loc='upper right')

plt.savefig('tablesize.pdf', format='pdf', bbox_inches='tight')


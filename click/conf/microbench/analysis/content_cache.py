#!/usr/bin/env python

from common import *

data_names = ('NO FB', 'FB1', 'CID REQ M', 'CID REQ H', 'CID REP')
times = map(lambda data_name: get_total_runtime(dataset[data_name] + '_timing'), data_names)
data = map(lambda x: packet/x/1000000, times)   # Mpkt/sec

indx = np.arange(len(data))
fig = plt.figure(figsize=(5, 5 * 0.6))
ax = fig.add_subplot(111)

width = 0.6

bars = ax.bar(indx, data, width, color = 'grey', zorder=3 , alpha =0.8)
ax.set_ylabel('Throughput (Mpkt/sec)')
ax.set_title('Per-hop processing throughput')
ax.set_xticks(indx+width/2)
ax.set_xticklabels(data_names)
ax.yaxis.grid(zorder=1)
ax.set_xlim(-0.2, (len(data) - 1) + width + 0.2)
ax.set_ylim(0, 3.7)

plt.savefig('content_cache.pdf', format='pdf', bbox_inches='tight')


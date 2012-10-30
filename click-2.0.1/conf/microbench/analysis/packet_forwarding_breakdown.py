#!/usr/bin/env python

from common import *
import plot_perf_breakdown
import re

##plot_perf_breakdown.plot('packet_forwarding_breakdown.pdf', ['IP', 'FB0', 'FB0-VIA', 'FB1', 'FB2', 'FB3', 'CID-REP'])
plot_perf_breakdown.plot('packet_forwarding_breakdown.pdf', ['IP', 'IP-FP', 'FB0', 'VIA', 'FB1', 'FB2', 'FB3', 'FB3-FP'])
#plot_perf_breakdown.plot('packet_forwarding_breakdown.pdf', ['FB0-INTRA-1', 'FB1-INTRA-1', 'FB2-INTRA-1', 'FB3-INTRA-1', 'FB0-INTRA-4', 'FB1-INTRA-4', 'FB2-INTRA-4', 'FB3-INTRA-4'])


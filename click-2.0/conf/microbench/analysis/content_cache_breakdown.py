#!/usr/bin/env python

from common import *
import plot_perf_breakdown

plot_perf_breakdown.plot('content_cache_breakdown.pdf', ['NO FB', 'FB1', 'CID REQ M', 'CID REQ H', 'CID REP'])


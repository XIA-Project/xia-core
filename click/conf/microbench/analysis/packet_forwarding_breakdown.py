#!/usr/bin/env python

from common import *
import plot_perf_breakdown
import re

plot_perf_breakdown.plot('packet_forwarding_breakdown.pdf', ['IP', 'FB0', 'FB0-VIA', 'FB1', 'FB2', 'CID-REP'])


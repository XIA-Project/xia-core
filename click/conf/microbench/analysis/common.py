import numpy as np
import matplotlib
matplotlib.use('agg')

import matplotlib.pyplot as plt
from matplotlib import rc

rc('font', **{'family' : 'sans-serif', 'serif': ['arial']})


import re

data_path_prefix = '../'
packet = int(re.search(r'define\(\$COUNT\s+(\d+)\)', open(data_path_prefix + 'common.inc').read()).group(1))

# excution time pattern
total_time_pat = re.compile(r'^([\d.]+)user .*?$')
first_packet_cputime_pat = re.compile(r'^FIRST_PACKET CPUTIME (\d+)$')
handler_call_cputime_pat = re.compile(r'^HANDLER_CALL CPUTIME (\d+)$')

def get_total_runtime(path):
    t = 0

    for line in open(path).readlines():
        mat = total_time_pat.match(line)
        if mat is not None:
            t += float(mat.group(1))

    return t

def get_processing_time(path):
    t = 0

    for line in open(path).readlines():
        mat = first_packet_cputime_pat.match(line)
        if mat is not None:
            t -= float(mat.group(1)) / 1000000000.
        mat = handler_call_cputime_pat.match(line)
        if mat is not None:
            t += float(mat.group(1)) / 1000000000.

    return t

# dataset
dataset = {
    'IP': data_path_prefix + 'output_ip_packetforward',
    'FB0': data_path_prefix + 'output_xia_packetforward_fallback0',
    'FB1': data_path_prefix + 'output_xia_packetforward_fallback1',
    'FB2': data_path_prefix + 'output_xia_packetforward_fallback2',
    'FB0-VIA': data_path_prefix + 'output_xia_packetforward_viapoint',
    'CID-REQ-M': data_path_prefix + 'output_xia_packetforward_cid_req_miss',
    'CID-REQ-H': data_path_prefix + 'output_xia_packetforward_cid_req_hit',
    'CID-REP': data_path_prefix + 'output_xia_packetforward_cid_rep',

    'TABLESIZE_CID_1': data_path_prefix + 'output_xia_tablesize_cid_1',
    'TABLESIZE_CID_3': data_path_prefix + 'output_xia_tablesize_cid_3',
    'TABLESIZE_CID_10': data_path_prefix + 'output_xia_tablesize_cid_10',
    'TABLESIZE_CID_30': data_path_prefix + 'output_xia_tablesize_cid_30',
    'TABLESIZE_CID_100': data_path_prefix + 'output_xia_tablesize_cid_100',
    'TABLESIZE_CID_300': data_path_prefix + 'output_xia_tablesize_cid_300',
    'TABLESIZE_CID_1000': data_path_prefix + 'output_xia_tablesize_cid_1000',
    'TABLESIZE_CID_3000': data_path_prefix + 'output_xia_tablesize_cid_3000',
    'TABLESIZE_CID_10000': data_path_prefix + 'output_xia_tablesize_cid_10000',
    'TABLESIZE_CID_30000': data_path_prefix + 'output_xia_tablesize_cid_30000',
    'TABLESIZE_CID_100000': data_path_prefix + 'output_xia_tablesize_cid_100000',
    'TABLESIZE_CID_300000': data_path_prefix + 'output_xia_tablesize_cid_300000',
    'TABLESIZE_CID_1000000': data_path_prefix + 'output_xia_tablesize_cid_1000000',
    'TABLESIZE_CID_3000000': data_path_prefix + 'output_xia_tablesize_cid_3000000',
    'TABLESIZE_CID_10000000': data_path_prefix + 'output_xia_tablesize_cid_10000000',
    'TABLESIZE_CID_30000000': data_path_prefix + 'output_xia_tablesize_cid_30000000',

    'TABLESIZE_AD_1': data_path_prefix + 'output_xia_tablesize_ad_1',
    'TABLESIZE_AD_3': data_path_prefix + 'output_xia_tablesize_ad_3',
    'TABLESIZE_AD_10': data_path_prefix + 'output_xia_tablesize_ad_10',
    'TABLESIZE_AD_30': data_path_prefix + 'output_xia_tablesize_ad_30',
    'TABLESIZE_AD_100': data_path_prefix + 'output_xia_tablesize_ad_100',
    'TABLESIZE_AD_300': data_path_prefix + 'output_xia_tablesize_ad_300',
    'TABLESIZE_AD_1000': data_path_prefix + 'output_xia_tablesize_ad_1000',
    'TABLESIZE_AD_3000': data_path_prefix + 'output_xia_tablesize_ad_3000',
    'TABLESIZE_AD_10000': data_path_prefix + 'output_xia_tablesize_ad_10000',
    'TABLESIZE_AD_30000': data_path_prefix + 'output_xia_tablesize_ad_30000',
    'TABLESIZE_AD_100000': data_path_prefix + 'output_xia_tablesize_ad_100000',
    'TABLESIZE_AD_300000': data_path_prefix + 'output_xia_tablesize_ad_300000',
    'TABLESIZE_AD_1000000': data_path_prefix + 'output_xia_tablesize_ad_1000000',
    'TABLESIZE_AD_3000000': data_path_prefix + 'output_xia_tablesize_ad_3000000',
    'TABLESIZE_AD_10000000': data_path_prefix + 'output_xia_tablesize_ad_10000000',
    'TABLESIZE_AD_30000000': data_path_prefix + 'output_xia_tablesize_ad_30000000',
}


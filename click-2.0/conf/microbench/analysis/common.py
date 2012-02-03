import numpy as np
import matplotlib
matplotlib.use('agg')

import matplotlib.pyplot as plt
from matplotlib import rc

rc('font', **{'family' : 'sans-serif', 'serif': ['arial']})


import re

data_path_prefix = '../'
packet = int(re.match(r'.*^define\(\$COUNT\s+(\d+)\).*', open(data_path_prefix + 'common.inc').read(), re.MULTILINE | re.DOTALL).group(1))

# excution time pattern
total_time_pat = re.compile(r'^([\d.]+)user .*?$')
first_packet_usertime_pat = re.compile(r'^FIRST_PACKET USERTIME (\d+)$')
handler_call_usertime_pat = re.compile(r'^HANDLER_CALL USERTIME (\d+)$')
mpps_pat = re.compile(r'^Mpps: ([\d.]+)$')

def get_total_runtime(path):
    t = 0

    print path
    for line in open(path).readlines():
        mat = total_time_pat.match(line)
        if mat is not None:
            t += float(mat.group(1))

    return t

def get_processing_time(path):
    t = 0

    for line in open(path).readlines():
        mat = first_packet_usertime_pat.match(line)
        if mat is not None:
            t -= float(mat.group(1)) / 1000000000.
        mat = handler_call_usertime_pat.match(line)
        if mat is not None:
            t += float(mat.group(1)) / 1000000000.

    return t

def get_pps(path):
    values = []

    for line in open(path).readlines():
        mat = mpps_pat.match(line)
        if mat is not None:
            values.append(float(mat.group(1)) * 1000000.)

    return sum(values[1:-1]) / (len(values) - 2)

# dataset
dataset = {
    'IP': data_path_prefix + 'output_ip_packetforward',
    'FB0': data_path_prefix + 'output_xia_packetforward_fallback0',
    'FB1': data_path_prefix + 'output_xia_packetforward_fallback1',
    'FB2': data_path_prefix + 'output_xia_packetforward_fallback2',
    'FB3': data_path_prefix + 'output_xia_packetforward_fallback3',
    'VIA': data_path_prefix + 'output_xia_packetforward_viapoint',
    'CID-REQ-M': data_path_prefix + 'output_xia_packetforward_cid_req_miss',
    'CID-REQ-H': data_path_prefix + 'output_xia_packetforward_cid_req_hit',
    'CID-REP': data_path_prefix + 'output_xia_packetforward_cid_rep',
    'FB3-FP': data_path_prefix + 'output_xia_packetforward_fallback3_fastpath',
    'IP-FP': data_path_prefix + 'output_ip_packetforward_fastpath',

    'TABLESIZE_HID_1': data_path_prefix + 'output_xia_tablesize_cid_1',
    'TABLESIZE_HID_3': data_path_prefix + 'output_xia_tablesize_cid_3',
    'TABLESIZE_HID_10': data_path_prefix + 'output_xia_tablesize_cid_10',
    'TABLESIZE_HID_30': data_path_prefix + 'output_xia_tablesize_cid_30',
    'TABLESIZE_HID_100': data_path_prefix + 'output_xia_tablesize_cid_100',
    'TABLESIZE_HID_300': data_path_prefix + 'output_xia_tablesize_cid_300',
    'TABLESIZE_HID_1000': data_path_prefix + 'output_xia_tablesize_cid_1000',
    'TABLESIZE_HID_3000': data_path_prefix + 'output_xia_tablesize_cid_3000',
    'TABLESIZE_HID_10000': data_path_prefix + 'output_xia_tablesize_cid_10000',
    'TABLESIZE_HID_30000': data_path_prefix + 'output_xia_tablesize_cid_30000',
    'TABLESIZE_HID_100000': data_path_prefix + 'output_xia_tablesize_cid_100000',
    'TABLESIZE_HID_300000': data_path_prefix + 'output_xia_tablesize_cid_300000',
    'TABLESIZE_HID_1000000': data_path_prefix + 'output_xia_tablesize_cid_1000000',
    'TABLESIZE_HID_3000000': data_path_prefix + 'output_xia_tablesize_cid_3000000',
    'TABLESIZE_HID_10000000': data_path_prefix + 'output_xia_tablesize_cid_10000000',
    'TABLESIZE_HID_30000000': data_path_prefix + 'output_xia_tablesize_cid_30000000',

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

iter_max = 10
#iter_max = 5
#iter_max = 2


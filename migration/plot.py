#!/usr/bin/env python

import re
import os

pat_ping = re.compile(r'^(\d+): PING sent; client seq = (\d+)$')
pat_pong = re.compile(r'^(\d+): PONG received; client seq = (\d+), server seq = (\d+)$')
pat_update = re.compile(r'^updating XIAPingSource with new address .*$')

# ignore first & last parts
trim_left = 1.0
trim_right = 1.0
# use 100 ms average
bucket_size = 0.100

def process(f):
    current_ping_dst = 0
    current_pong_src = 0

    first = None

    ping_to = [[], []]
    pong_from = [[], []]

    for line in f:
        line = line.strip()

        mat = pat_ping.match(line)
        if mat is not None:
            t = int(mat.group(1))
            if first is None:
                first = t + trim_left
            t = (t - first) / 1000000.
            if t >= 0:
                ping_to[current_ping_dst].append(t)

        mat = pat_pong.match(line)
        if mat is not None:
            t = int(mat.group(1))
            t = (t - first) / 1000000.
            if t >= 0:
                pong_from[current_pong_src].append(t)

        mat = pat_update.match(line)
        if mat is not None:
            current_ping_dst = 1 - current_ping_dst
            current_pong_src = 1 - current_pong_src

    # calculate avg rate
    buckets = []
    for v in pong_from[0] + pong_from[1]:
        bucket_idx = int(v / bucket_size)
        if len(buckets) <= bucket_idx:
            buckets += [0.] * (bucket_idx - len(buckets) + 1)
        buckets[bucket_idx] += 1. / bucket_size

    # drop last part of data
    last = max(ping_to[0] + ping_to[1])
    ping_to[0] = filter(lambda x: last - x > trim_right, ping_to[0])
    ping_to[1] = filter(lambda x: last - x > trim_right, ping_to[1])
    pong_from[0] = filter(lambda x: last - x > trim_right, pong_from[0])
    pong_from[1] = filter(lambda x: last - x > trim_right, pong_from[1])
    buckets = buckets[:-int(trim_right / bucket_size)]

    # write avg rate & points
    f = open('plot.dat', 'w')
    for bucket_idx, rate in enumerate(buckets):
        f.write('%f %f\n' % (bucket_idx * bucket_size, rate))
    f.write('\n\n')
    f.write('\n'.join([str(x) for x in ping_to[0]]) + '\n\n\n')
    f.write('\n'.join([str(x) for x in ping_to[1]]) + '\n\n\n')
    f.write('\n'.join([str(x) for x in pong_from[0]]) + '\n\n\n')
    f.write('\n'.join([str(x) for x in pong_from[1]]) + '\n\n\n')


if __name__ == '__main__':
    process(open('output'))
    os.system('./plot.gnuplot')


#!/usr/bin/env python

import re
import os

pat_client_ping = re.compile(r'^(\d+): PING sent; client seq = (\d+)$')
pat_client_pong = re.compile(r'^(\d+): PONG received; client seq = (\d+), server seq = (\d+)$')
pat_client_update = re.compile(r'^(\d+): updating XIAPingSource with new address .*$')

pat_server_ping = re.compile(r'^(\d+): PING received; client seq = (\d+)$')
pat_server_pong = re.compile(r'^(\d+): PONG sent; client seq = (\d+), server seq = (\d+)$')
pat_server_update = re.compile(r'^(\d+): updating XIAPingResponder with new address .*$')

# use 250 ms average
bucket_size = 0.250
# use 1 out of 10 points
sampling_rate = 10

def process(f_client, f_server):
    current_ping_dst = 0
    current_pong_src = 0

    client_first_t = None
    client_first_seq = None
    server_first_t = None
    server_first_seq = None
    last = 0

    client_ping = [[], []]
    client_pong = [[], []]
    server_ping = [[], []]
    server_pong = [[], []]

    output_client = list(f_client.readlines())
    output_server = list(f_server.readlines())

    for line in output_server:
        mat = pat_server_pong.match(line.strip())
        if mat is None:
            continue

        server_first_t = int(mat.group(1)) / 1000000.
        client_first_seq = int(mat.group(2))
        server_first_seq = int(mat.group(3))

        for line in output_client:
            mat = pat_client_ping.match(line.strip())
            if mat is None:
                continue

            client_seq = int(mat.group(2))
            if client_first_seq == client_seq:
                client_first_t = int(mat.group(1)) / 1000000.
                break
        break

    for line in output_client + output_server:
        line = line.strip()

        mat = pat_client_ping.match(line)
        if mat is not None:
            t = int(mat.group(1)) / 1000000. - client_first_t
            seq = int(mat.group(2)) - client_first_seq
            if t >= 0: client_ping[current_ping_dst].append((t, seq))
            if last < t: last = t

        mat = pat_client_pong.match(line)
        if mat is not None:
            t = int(mat.group(1)) / 1000000. - client_first_t
            seq = int(mat.group(2)) - client_first_seq
            seq2 = int(mat.group(3)) - server_first_seq
            if t >= 0: client_pong[current_pong_src].append((t, seq, seq2))
            if last < t: last = t

        mat = pat_server_ping.match(line)
        if mat is not None:
            t = int(mat.group(1)) / 1000000. - server_first_t
            seq = int(mat.group(2)) - client_first_seq
            if t >= 0: server_ping[current_ping_dst].append((t, seq))
            if last < t: last = t

        mat = pat_server_pong.match(line)
        if mat is not None:
            t = int(mat.group(1)) / 1000000. - server_first_t
            seq = int(mat.group(2)) - client_first_seq
            seq2 = int(mat.group(3)) - server_first_seq
            if t >= 0: server_pong[current_pong_src].append((t, seq, seq2))
            if last < t: last = t

        mat = pat_client_update.match(line)
        if mat is not None:
            current_ping_dst = 1 - current_ping_dst

        mat = pat_server_update.match(line)
        if mat is not None:
            current_pong_src = 1 - current_pong_src

    # calculate avg rate
    buckets = []
    for v in client_pong[0] + client_pong[1]:
        bucket_idx = int(v[0] / bucket_size)
        if len(buckets) <= bucket_idx:
            buckets += [0.] * (bucket_idx - len(buckets) + 1)
        buckets[bucket_idx] += 1. / bucket_size

    # write avg rate & points
    f = open('plot.dat', 'w')

    for bucket_idx, rate in enumerate(buckets):
        f.write('%f %f\n' % (bucket_idx * bucket_size, rate))
    f.write('\n\n')

    for group in client_ping:
        for t, seq in group:
            if seq % sampling_rate == 0:
                f.write('%f %d\n' % (t, seq))
        f.write('\n\n')
    for group in client_pong:
        for t, seq, seq2 in group:
            if seq % sampling_rate == 0:
                f.write('%f %d %d\n' % (t, seq, seq2))
        f.write('\n\n')
    for group in server_ping:
        for t, seq in group:
            if seq % sampling_rate == 0:
                f.write('%f %d\n' % (t, seq))
        f.write('\n\n')
    for group in server_pong:
        for t, seq, seq2 in group:
            if seq % sampling_rate == 0:
                f.write('%f %d %d\n' % (t, seq, seq2))
        f.write('\n\n')


if __name__ == '__main__':
    process(open('output_client'), open('output_server'))
    os.system('./plot.gnuplot')


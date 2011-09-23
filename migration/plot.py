#!/usr/bin/env python

import re
import os

pat_time = re.compile(r'^(\d+): .*$')

pat_client_ping = re.compile(r'^(\d+): PING sent; client seq = (\d+)$')
pat_client_pong = re.compile(r'^(\d+): PONG received; client seq = (\d+), server seq = (\d+)$')
pat_client_update = re.compile(r'^(\d+): updating XIAPingSource with new address .*$')

pat_server_ping = re.compile(r'^(\d+): PING received; client seq = (\d+)$')
pat_server_pong = re.compile(r'^(\d+): PONG sent; client seq = (\d+), server seq = (\d+)$')
pat_server_update = re.compile(r'^(\d+): updating XIAPingResponder with new address .*$')

#bucket_size = 0.050 # in seconds
averaging_window = 0.250
averaging_window_step = 0.001
sampling_rate = 1

rtt = 0.200 / 1000      # known rtt between client & server

def process(f_client, f_server):
    current_ping_dst = 0
    current_pong_src = 0

    client_first_t = None
    client_first_seq = None
    server_first_t = None
    server_first_seq = None
    last = 0
    last_reception = 0
    freeze_t = None

    client_ping = [[], []]
    client_pong = [[], []]
    server_ping = [[], []]
    server_pong = [[], []]
    client_update = []
    server_update = []

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
                client_first_t = int(mat.group(1)) / 1000000. - rtt / 2.
                break
        break

    while True:
        if output_client and output_server:
            mat_client = pat_time.match(output_client[0].strip())
            if mat_client is None:
                del output_client[0]
                continue

            mat_server = pat_time.match(output_server[0].strip())
            if mat_server is None:
                del output_server[0]
                continue

            client_t = int(mat_client.group(1)) / 1000000. - client_first_t
            server_t = int(mat_server.group(1)) / 1000000. - server_first_t

            sel_client = client_t < server_t
        elif output_client:
            sel_client = True
        elif output_server:
            sel_client = False
        else:
            break

        if sel_client:
            line = output_client[0].strip()
            del output_client[0]
        else:
            line = output_server[0].strip()
            del output_server[0]

        mat = pat_client_ping.match(line)
        if mat is not None:
            t = int(mat.group(1)) / 1000000. - client_first_t
            seq = int(mat.group(2)) - client_first_seq
            if t >= 0: client_ping[current_ping_dst].append((t, seq))

        mat = pat_client_pong.match(line)
        if mat is not None:
            t = int(mat.group(1)) / 1000000. - client_first_t
            seq = int(mat.group(2)) - client_first_seq
            seq2 = int(mat.group(3)) - server_first_seq
            if t >= 0: client_pong[current_pong_src].append((t, seq, seq2))
            if last_reception > 0 and t - last_reception > 0.100 and freeze_t is None:
                freeze_t = last_reception
            else: last_reception = t
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
            t = int(mat.group(1)) / 1000000. - client_first_t
            if t >= 0: client_update.append(t)
            current_ping_dst = 1 - current_ping_dst

        mat = pat_server_update.match(line)
        if mat is not None:
            t = int(mat.group(1)) / 1000000. - server_first_t
            if t >= 0: server_update.append(t)
            current_pong_src = 1 - current_pong_src

    # calculate avg rate
    #buckets = []
    #for v in client_pong[0] + client_pong[1]:
    #    bucket_idx = int(v[0] / bucket_size)
    #    if len(buckets) <= bucket_idx:
    #        buckets += [0.] * (bucket_idx - len(buckets) + 1)
    #    buckets[bucket_idx] += 1. / bucket_size
    rates = []
    window = []
    t = 0
    rate_points = list(sorted(client_pong[0] + client_pong[1]))
    while rate_points:
        while window and t - window[0][0] > averaging_window:
            del window[0]
        while rate_points:
            if rate_points[0][0] <= t:
                window.append(rate_points[0])
                del rate_points[0]
            else:
                break
        if window:
            current_rate = len(window) / averaging_window
            rates.append((t, current_rate))
        t += averaging_window_step

    # write avg rate & points
    f = open('plot.dat', 'w')

    #for bucket_idx, rate in enumerate(buckets):
    #    f.write('%f %f\n' % ((bucket_idx + 0.5) * bucket_size, rate))
    #f.write('\n\n')
    for t, rate in rates:
        f.write('%f %f\n' % (t, rate))
    f.write('\n\n')

    for i, group in enumerate(client_ping):
        f.write('# client_ping %d\n' % i)
        for t, seq in group:
            if seq % sampling_rate == 0:
                f.write('%f %d\n' % (t, seq))
        f.write('\n\n')
    for i, group in enumerate(client_pong):
        f.write('# client_pong %d\n' % i)
        for t, seq, seq2 in group:
            if seq % sampling_rate == 0:
                f.write('%f %d %d\n' % (t, seq, seq2))
        f.write('\n\n')
    for i, group in enumerate(server_ping):
        f.write('# server_ping %d\n' % i)
        for t, seq in group:
            if seq % sampling_rate == 0:
                f.write('%f %d\n' % (t, seq))
        f.write('\n\n')
    for i, group in enumerate(server_pong):
        f.write('# server_pong %d\n' % i)
        for t, seq, seq2 in group:
            if seq % sampling_rate == 0:
                f.write('%f %d %d\n' % (t, seq, seq2))
        f.write('\n\n')

    for t in [freeze_t]:
        f.write('%f 0\n' % t)
        f.write('%f 100000\n' % t)
    f.write('\n\n')

    for t in client_update:
        f.write('%f 0\n' % t)
        f.write('%f 100000\n' % t)
    f.write('\n\n')

    for t in server_update:
        f.write('%f 0\n' % t)
        f.write('%f 100000\n' % t)
    f.write('\n\n')


if __name__ == '__main__':
    process(open('output_client'), open('output_server'))
    os.system('./plot.gnuplot')


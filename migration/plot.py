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

# known rtt between client & server
rtt = 25. / 1000

def find_server_first_t(output_server, client_first_t, client_last_t, client_last_seq):
    for trial, line in enumerate(output_server):
        if trial > 100:
            break

        mat = pat_server_pong.match(line.strip())
        if mat is None:
            continue

        if int(mat.group(2)) == client_last_seq:
            server_last_t = int(mat.group(1)) / 1000000.
            return server_last_t - (client_last_t - client_first_t) + rtt / 2.

    # packet might be lost; need to stick to the previous result
    return None

def process(f_client, f_server):
    last_reception = 0
    freeze_t = None

    client_ping = []
    client_pong = []
    client_update = []

    client_seq_to_time_map = {}

    server_ping = []
    server_pong = []
    server_update = []

    output_client = list(f_client.readlines())
    output_server = list(f_server.readlines())

    client_first_t = None
    server_first_t = None

    while output_client:
        line = output_client[0].strip()

        mat = pat_time.match(output_client[0])
        if mat is None:
            del output_client[0]
            continue

        t = int(mat.group(1)) / 1000000.
        if client_first_t is None:
            client_first_t = t
        t -= client_first_t

        mat = pat_client_ping.match(line)
        if mat is not None:
            seq = int(mat.group(2))
            if t >= 0:
                client_ping.append((t, seq))
            client_seq_to_time_map[seq] = t

        mat = pat_client_pong.match(line)
        if mat is not None:
            seq = int(mat.group(2))
            seq2 = int(mat.group(3))
            if t >= 0:
                client_pong.append((t, seq, seq2))

        mat = pat_client_update.match(line)
        if mat is not None:
            if t >= 0:
                client_update.append(t)

        del output_client[0]

    def discover_server_first_t():
        i = 0
        while output_server:
            if len(output_server) <= i:
                break
            line = output_server[i].strip()
            i += 1

            mat = pat_server_ping.match(line)
            if mat is not None:
                t = int(mat.group(1)) / 1000000.
                server_first_t = t - (client_seq_to_time_map[int(mat.group(2))] - client_seq_to_time_map[0]) - rtt / 2.
                break

            mat = pat_server_pong.match(line)
            if mat is not None:
                t = int(mat.group(1)) / 1000000.
                server_first_t = t - (client_seq_to_time_map[int(mat.group(2))] - client_seq_to_time_map[0]) - rtt / 2.
                break

        return server_first_t

    while output_server:
        line = output_server[0].strip()

        mat = pat_time.match(output_server[0])
        if mat is None:
            del output_server[0]
            continue

        t = int(mat.group(1)) / 1000000.
        #if server_first_t is None:
        server_first_t = discover_server_first_t()
        t -= server_first_t

        if last_reception > 0 and t - last_reception > 0.100 and freeze_t is None:
            freeze_t = last_reception
            # resync server time
            server_first_t = None
            #print 'resync %f %f' % (last_reception, t)
            continue
        else:
            last_reception = t

        mat = pat_server_ping.match(line)
        if mat is not None:
            seq = int(mat.group(2))
            if t >= 0:
                server_ping.append((t, seq))

        mat = pat_server_pong.match(line)
        if mat is not None:
            seq = int(mat.group(2))
            seq2 = int(mat.group(3))
            if t >= 0:
                server_pong.append((t, seq, seq2))

        mat = pat_server_update.match(line)
        if mat is not None:
            if t >= 0:
                server_update.append(t)

        del output_server[0]


    rates = []
    window = []
    t = 0
    rate_points = list(sorted(client_pong))
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

    for t, rate in rates:
        f.write('%f %f\n' % (t, rate))
    f.write('\n\n')

    f.write('# client_ping\n')
    assert(client_ping)
    for t, seq in client_ping:
        if seq % sampling_rate == 0:
            f.write('%f %d\n' % (t, seq))
    f.write('\n\n')

    f.write('# server_ping\n')
    assert(server_ping)
    for t, seq in server_ping:
        if seq % sampling_rate == 0:
            f.write('%f %d\n' % (t, seq))
    f.write('\n\n')

    f.write('# server_pong\n')
    assert(server_pong)
    for t, seq, seq2 in server_pong:
        if seq % sampling_rate == 0:
            f.write('%f %d %d\n' % (t, seq, seq2))
    f.write('\n\n')

    f.write('# client_pong\n')
    assert(client_pong)
    for t, seq, seq2 in client_pong:
        if seq % sampling_rate == 0:
            f.write('%f %d %d\n' % (t, seq, seq2))
    f.write('\n\n')

    #f.write('%f 0\n' % freeze_t)
    #f.write('%f 100000\n' % freeze_t)
    #f.write('\n\n')

    #f.write('%f 0\n' % server_update[0])
    #f.write('%f 100000\n' % server_update[0])
    #f.write('\n\n')

    #for t in client_update:
    #    f.write('%f 0\n' % client_update[0])
    #    f.write('%f 100000\n' % client_update[0])
    #f.write('\n\n')

    xmin = freeze_t - (client_update[0] - freeze_t) / 2.
    xmax = client_update[0] + (client_update[0] - freeze_t) / 2.
    scale = 1000
    print 'set xrange [%f:%f]' % (0, (xmax - xmin) * scale)
    print 'offset = %f' % (xmin * scale)
    print 'freeze = %f' % ((freeze_t - xmin) * scale)
    print 'server_update = %f' % ((server_update[0] - xmin) * scale)
    print 'client_update = %f' % ((client_update[0] - xmin) * scale)


if __name__ == '__main__':
    process(open('output_client'), open('output_server'))
    os.system('./plot.gnuplot')


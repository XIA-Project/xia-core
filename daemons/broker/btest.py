import sys
import time
import socket
import struct
import random
import logging

import cdn_pb2
from scenario import Scenario

def request_msg(host, bitrate):
    msg = cdn_pb2.CDNMsg()
    msg.version = cdn_pb2.CDN_PROTO_VERSION
    msg.type = cdn_pb2.CDN_REQUEST_MSG
    msg.client = host
    msg.request.bitrate = bitrate
    return msg



def scores_msg(host, scores):
    msg = cdn_pb2.CDNMsg()
    msg.version = cdn_pb2.CDN_PROTO_VERSION
    msg.type = cdn_pb2.PING_SCORES_MSG

    msg.client = host

    for (name, rtt, loss) in scores:
        c = msg.scores.clusters.add()
        c.name = name
        c.rtt = rtt
        c.loss = loss

    return msg



def send_msg(sock, msg):
    msgstr = msg.SerializeToString()
    length = len(msgstr)

    sock.send(struct.pack("!L", length))

    sent = 0
    while sent < length:
        sent += sock.send(msgstr[sent:])



def recv_msg(sock):
    msgstr = ''
    received = 0

    length = struct.unpack("!L", sock.recv(4))[0]
    while received < length:
        msgstr += sock.recv(length - received)
        received = len(msgstr)

    msg = cdn_pb2.CDNMsg()
    msg.ParseFromString(msgstr)
    return msg



def connect(addr, port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('localhost', 44444))
    return s



def get_cluster(host, bitrate):
    sock = connect('localhost', 9999)
    send_msg(sock, request_msg(host, bitrate))
    msg = recv_msg(sock)
    sock.close()
    return msg.request.cluster



def send_scores(host, scores):
    sock = connect('localhost', 9999)
    send_msg(sock, scores_msg(host, scores))
    sock.close()


def prime_scores():
    for id, location in scenario.scenario['client_locations'].iteritems():
        scores = []
        for cluster_id, cluster in scenario.scenario['cdn_locations'].iteritems():
            score = [cluster['name'], random.randrange(0,100), random.randrange(0,100,10)]
            scores.append(score)
        print scores
        send_scores(location['name'], scores)


if __name__ == '__main__':
    random.seed
    log = logging.getLogger()
    log.setLevel(logging.DEBUG)

    scenario = Scenario(open('../../etc/scenario.conf'))
    prime_scores()
    count = 0
    while True:
        id = random.choice(scenario.scenario['client_locations'].keys())
        client = scenario.scenario['client_locations'][id]['name']
        bitrate = random.choice(scenario.scenario['bitrates'].keys())


        cluster = get_cluster(client, bitrate)
        logging.info('%s at %d got %s' % (client, bitrate, cluster))
        time.sleep(1)

        count += 1
        if count == 15:
            count = 0
            prime_scores()


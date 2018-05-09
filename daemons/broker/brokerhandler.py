#! /usr/bin/env python
# Copyright 2018 Carnegie Mellon University
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import struct
import random
import logging
import SocketServer

import cdn_pb2
from scenario import Scenario

# FIXME: should bandwidth be incorporated in what we return from here, or is it only for future calcuations?

class BrokerHandler(SocketServer.BaseRequestHandler):
    scenario = None

    #
    # handle incoming requests & validate protobuf
    #
    def handle(self):
        # get the length of the msg
        length = struct.unpack("!L", self.request.recv(4))[0]

        try:
            msg = cdn_pb2.CDNMsg()
            msg.ParseFromString(self.request.recv(length))

            if msg.type == cdn_pb2.CDN_REQUEST_MSG:
                self.handle_request(msg)
            elif msg.type == cdn_pb2.PING_SCORES_MSG or msg.type == cdn_pb2.STATS_SCORE_MSG:
                self.handle_scores(msg)
            else:
                logging.warn('unknown msg type')
        except Exception as err:
            logging.warn('invalid message data')



    #
    # handle requests for a target CDN
    #
    def handle_request(self, msg):
        logging.debug('finding optimal cluster for %s' % msg.client)

        id = self.scenario.getID(msg.client)

        # add a new request entry
        self.scenario.add_request(id, msg.request.bandwidth, msg.request.last_cdn)

        # find the best cluster for this client
        # last_cdn and bitrate not currently used here
        (cluster, ad, hid) = self.scenario.get_optimal_cluster(id, msg.request.bandwidth, msg.request.last_cdn)

        logging.info('Returning cluster: %s %s %s' % (cluster, ad, hid))

        reply_msg = cdn_pb2.CDNMsg()
        reply_msg.version = cdn_pb2.CDN_PROTO_VERSION
        reply_msg.type = cdn_pb2.CDN_REPLY_MSG
        reply_msg.reply.cluster = cluster
        reply_msg.reply.ad = ad
        reply_msg.reply.hid = hid

        self.send_message(reply_msg)



    #
    # handle client score updates
    #
    def handle_scores(self, msg):
        logging.info('handling client update from %s' % msg.client)
        self.scenario.update_scores(msg)



    #
    # send a protobuf msg reply
    #
    def send_message(self, msg):
        msgstr = msg.SerializeToString()
        length = len(msgstr)
        self.request.sendall(struct.pack("!L", length))
        self.request.sendall(msgstr)




if __name__ == "__main__":
    HOST, PORT = "localhost", 44444
    log = logging.getLogger()
    log.setLevel(logging.DEBUG)

    f = open('../../etc/scenario.conf')
    s = Scenario(f)

    s.scenario['accepted_bids'] = {}

    SocketServer.TCPServer.allow_reuse_address = True
    server = SocketServer.TCPServer((HOST, PORT), BrokerHandler)
    server.RequestHandlerClass.scenario = s
    server.serve_forever()


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
import logging
import SocketServer

import cdn_pb2
from scenario import Scenario

# FIXME: should bitrate be incorporated in what we return from here, or is it only for future calcuations?

class BrokerHandler(SocketServer.BaseRequestHandler):
    scenario = None

    def handle(self):
        # get the length of the msg
        length = struct.unpack("!L", self.request.recv(4))[0]

        try:
            msg = cdn_pb2.CDNMsg()
            msg.ParseFromString(self.request.recv(length))

            if msg.type == cdn_pb2.CDN_REQUEST_MSG:
                self.handle_request(msg)
            elif msg.type == cdn_pb2.PING_SCORES_MSG:
                self.handle_scores(msg)
            else:
                logging.warn('unknown msg type')
        except Exception as err:
            logging.warn('invalid message data')



    def handle_request(self, msg):
        logging.debug('computing optimal cluster for %s' % msg.client)

        try:
            id = self.scenario.getID(msg.client)

            # add a new request entry
            self.scenario.add_request(id, msg.request.bitrate)

            # fetch the bid from accepted_bids
            bids = self.scenario.scenario['accepted_bids']
            cluster_id = 0

            # FIXME: we need live data for testing
            #dag = self.scenario.scenario['cdn_locations'][cluster_id]['dag']
            dag = 'AD:1234567890 HID:1234567890'


        except Exception as err:
            logging.debug('no valid bids found for %s' % client)
            dag = ''

        logging.debug('Returning dag: %s' % dag)
        msg.request.cluster = dag
        self.send_message(msg)



    def handle_scores(self, msg):
        logging.info('handling client update from %s' % msg.client)

        id = self.scenario.getID(msg.client)

        # find or create new client_location entry
        client = self.scenario.scenario['client_locations'][id]
        if client == None:
            # FIXME: what do we do about lat/long?
            client = make_client_record(msg.client, id, 0.0, 0.0)
            self.scenario.scenario['client_locations'][id] = client

        # reset score  list
        client['cluster_scores'] = []

        for cluster in msg.scores.clusters:
            cluster_id = self.scenario.getID(cluster.name)
            rtt = cluster.rtt
            loss = cluster.loss

            logging.debug('cluster %s (%d) has %s %s' % (cluster.name, cluster_id, rtt, loss))

            # first pass at making score score = (latency * 1000) + (packet loss percentage * 100)
            # lower is better

            if rtt == None or rtt <= 0 or loss == 100:
                score = 10000000000
            else:
                score = (rtt * 1000) + (loss * 100)
            client['cluster_scores'].append([cluster_id, score])

        logging.debug(client)


    def send_message(self, msg):
        msgstr = msg.SerializeToString()
        length = len(msgstr)
        self.request.send(struct.pack("!L", length), 4)
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


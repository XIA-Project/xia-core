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

import msgpack
import rpyc
from rpyc.utils.server import ThreadedServer

import cdn_pb2
import brokerserver
from scenario import Scenario



class BrokerServer(rpyc.Service):
    #
    # collect ping stats from client machines
    # stats include ping latency and packet loss info from the client to each cluster
    #
    def exposed_update_client_stats(self, stats):
        data = msgpack.unpackb(stats)
        logging.info('handling client update from %s' % data['name'])

        id = int(data['id'])

        # find or create new client_location entry
        client = scenario.scenario['client_locations'][id]
        if client == None:
            client = make_client_record(stats['name'], stats['id'], stats['lat'], stats['lon'])
            scenario.scenario['client_locations'][id] = client

        # reset score  list
        client['cluster_scores'] = []

        for cluster, info  in data['clusters'].iteritems():
            cluster_id = scenario.getID(cluster)
            logging.debug('cluster %s (%d) has %s %s' % (cluster, cluster_id, info['latency'], info['loss']))

            # first pass at making score score = (latency * 1000) + (packet loss percentage * 100)
            # lower is better
            score = info['latency']
            loss = info['loss']

            if score == None or score < 0 or loss == 100:
                score = 10000000000
            else:
                score *= 1000
                score += (loss * 100)
            client['cluster_scores'].append([cluster_id, score])

        logging.debug(scenario.scenario['client_locations'])


    #
    # get cache DAG from cdn hosts
    #
    def exposed_add_cdn_cluster(self, name, dag):
        logging.debug('adding dag to %s (%s)', name, dag)
        id = scenario.getID(name)
        scenario.scenario['cdn_locations'][id]['dag'] = dag
        logging.debug(scenario.scenario['cdn_locations'][id])

    def exposed_get_cdn(self, name, bitrate):
        logging.debug("handling request from %s for %s at %d" % (name, item, bitrate))
        id = scenario.getID(client)
        req = scenario['client_locations'][id]

        req['bitrate'] = bitrate
        scenario['requests'] = [req]
        # run the algorithm
        # return something


    def exposed_get_cdn(self, client, bitrate):
        id = scenario.getID(client)

        logging.debug('computing optimal cluster for %s' % client)

        try:
            # fetch the bid from accepted_bids
            id = scenario.getID(client)
            bids = scenario.scenario['accepted_bids']
            cluster_id = 0
            dag = scenario.scenario['cdn_locations'][cluster_id]['dag']

        except:
            logging.debug('no valid bids found for %s' % client)
            dag = ''

        print dag
        return dag


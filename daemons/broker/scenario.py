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

import sys
import time
import random
import logging
from configparser import ConfigParser

import cdn_pb2
MAX_REQUEST_AGE = 60

class Scenario:
    def __init__(self, fname):
        self.allow_cached = True
        self.bid_mode = 'both'
        self.populate(fname)


    #
    # set factors used to do bid calculations in the vdx engine
    #
    def set_mode(self, mode):
        self.bid_mode = mode


    #
    # get the bid mode
    #
    def mode(self):
        return self.bid_mode



    #
    # if false, exclude any results that were served from a CID cache
    #
    def allow_cached_fetches(self, allow):
        self.allow_cached = allow



    #
    # return the scenario id for the specified host name
    #
    def getID(self, name):
        return self.scenario['ids'][name]


    #
    # load scenario information from file
    #
    def populate(self, fname):
        self.scenario = {}
        self.scenario['cdn_locations'] = {}
        self.scenario['bitrates'] = {}
        self.scenario['CDNs'] = []
        self.scenario['client_locations'] = {}
        self.scenario['requests'] = []
        self.scenario['capacities'] = {}
        self.scenario['median_capacity'] = {}
        self.scenario['CDN_standard_price'] = []
        self.scenario['ids'] = {}
        self.scenario['accepted_bids'] = {}

        try:
            self.config = ConfigParser(allow_no_value = True)
            self.config.readfp(fname)
        except Exception as err:
            logging.warning(str(err))
            sys.exit(-1)

        self.load_bitrates()
        self.load_cdns()
        self.load_client_locations()


    #
    # fill in client record (request/location)
    # FIXME: can probably eliminate most if not all of the blank fields
    #
    def make_client_record(self, name, id):
        logging.debug('creating client record for %s' % name)
        client= {}

        lat = self.config.getfloat(name, 'lat')
        lon = self.config.getfloat(name, 'lon')

        client['name'] = name
        client['mgID'] = id
        client['lat'] = lat
        client['lon'] = lon
        client['timestamp'] = int(time.time())

        client['max_tput'] = 0.0

        client['cluster_scores'] = []
        client['cluster_stats'] = []
        client['id'] = ''

        client['asn'] = ''
        client['bitrate'] = 0
        client['cdn'] = ''
        client['city'] = ''
        client['client_id'] = ''
        client['connection_type'] = ''
        client['country'] = ''
        client['customer'] = ''
        client['initial_bitrate'] = ''
        client['initial_cdn'] = ''
        client['new_session'] = False
        client['object_id'] = ''
        client['regression'] = []
        client['u0'] = ''
        client['u1'] = ''
        client['u2'] = ''
        client['u3'] = ''
        client['u4'] = ''
        client['u17'] = ''

        return client


    #
    # load static client information
    #
    def load_client_locations(self):
        logging.debug('loading clients')
        clients = {}

        client_list = self.config.items('clients')
        for (client, value)  in client_list:
            # we only want bare keys
            if value != None and value != '':
                continue

            logging.debug('loading client %s' % client)

            id = self.config.getint(client, 'id')
            location = self.make_client_record(client, id)

            # give every client cluster pair a large default starting value
            for idc in self.scenario['cdn_locations'].keys():
                location['cluster_scores'].append([idc, 999999, 0])
                stats = {}
                stats['cached'] = 0
                stats['bitrate'] = 0
                stats['tput'] = 0.0
                location['cluster_stats'].append([idc, stats])

            self.scenario['client_locations'][id] = location
            self.scenario['ids'][client] = id

    #
    # add a new entry to the requests table
    # assumes that there is a matching client_location entry
    #
    def add_request(self, id, bitrate, last_cdn):
        try:
            request = dict(self.scenario['client_locations'][id])
            #request['bitrate'] = bitrate
            request['timestamp'] = int(time.time())
            request['cdn'] = last_cdn
            request['bitrate'] = bitrate
            self.scenario['requests'].append(request)
            logging.debug('added new request for %d at %d' % (id, bitrate))
        except Exception as err:
            logging.warning('unable to add request for %d %d' % (id, bitrate))
            logging.warning(str(err))


    #
    # for debugging, create a random request
    #
    def add_random_request(self):
        # make up a random request
        client = random.choice(self.scenario['client_locations'].keys())
        bitrate = random.choice(self.scenario['bitrates'].keys())
        self.add_request(client, bitrate)


    #
    # load top level CDNs
    #
    def load_cdns(self):
        logging.debug('loading CDNs')
        cdns = []
        median_capacity = []

        cdn_list = self.config.items('cdns')
        for cdn, value in cdn_list:
            # we only want bare keys
            if value != None and value != '':
                continue

            logging.debug('loading CDN %s' % cdn)
            clusters = []
            cdn_id = self.config.getint(cdn, 'id')
            self.scenario['ids'][cdn] = cdn_id

            for (cluster, value) in self.config.items(cdn):
                # we only want bare keys
                if value != None and value != '':
                    continue
                cluster_id = self.config.getint(cluster, 'id')
                capacity = self.config.getint(cluster, 'capacity')

                clusters.append(cluster_id)
                self.load_cluster_location(cluster)

                self.scenario['ids'][cluster] = cluster_id

                self.scenario['capacities'][(cdn_id, cluster_id)] = self.max_bitrate * capacity


                # FIXME see vdx.py line 174
                # is this the right thing to do here??
                # since we have small #'s of connection in the scenario, don't scale anything up
                median_capacity.append(self.scenario['capacities'][(cdn_id, cluster_id)])

            cdns.append(clusters)
        self.scenario['CDNs'] = cdns


    #
    # load CDN cluster location data
    # FIXME: can probably eliminate most if not all of the empty fields
    #
    def load_cluster_location(self, cluster):
        logging.debug('loading cluster location for %s' % cluster)
        location =  {}

        id = self.config.getint(cluster, 'id')
        location['id'] = id
        location['lat'] = self.config.getfloat(cluster, 'lat')
        location['lon'] = self.config.getfloat(cluster, 'lon')
        location['dag'] = self.config.get(cluster, 'dag')
        location['ad'] = self.config.get(cluster, 'ad')
        location['hid'] = self.config.get(cluster, 'hid')
        location['name'] = cluster

        # FIXME: what does the cost need to look like? is our simple value good enough?
        location['bw_cost'] = self.config.getfloat(cluster, 'cost')

        # FIXME: do I need any of these?
        location['city'] = ''
        location['state'] = ''
        location['country'] = ''
        location['isp'] = ''
        location['ecore'] = ''
        location['colo_cost'] = 1.0

        self.scenario['cdn_locations'][id] = location




    #
    # load bitrate info
    # FIXME: not sure this really needs to be configured in the scenario file
    #
    def load_bitrates(self):
        logging.debug('loading bitrate tables')
        bitrates = {}
        for (bitrate, value)  in self.config.items('bitrates'):
            bitrates[int(bitrate)] = float(value)
        self.scenario['bitrates'] = bitrates
        self.max_bitrate = sorted(bitrates.keys())[-1]


    #
    # clear out old requests
    #
    def prune_requests(self):
        now = int(time.time())
        requests = self.scenario['requests']
        last = -1

        for i in range(len(requests)):
            request = self.scenario
            timestamp = requests[i]['timestamp']

            # stop once we find an unexpired request
            if now - timestamp < MAX_REQUEST_AGE:
                last = i
                break

        if last < 0:
            self.scenario['requests'] = []
            length = 0
            logging.debug('the request table is empty')
        else:
            self.scenario['requests'] = requests[last:]
            length = len(requests)
            logging.debug('there are %d requests in the queue' % length)

        return length


    def get_optimal_cluster(self, client_id, bitrate, last_cdn):
        try:
            bid = self.scenario['accepted_bids'][client_id]

            logging.debug(bid)
            cluster_id = bid[1]
            #dag = self.scenario['cdn_locations'][cluster_id]['dag']
            ad = self.scenario['cdn_locations'][cluster_id]['ad']
            hid = self.scenario['cdn_locations'][cluster_id]['hid']
            name = self.scenario['cdn_locations'][cluster_id]['name']

        except Exception as err:
            # we couldn't find a bid, pick a cluster at random
            # FIXME: change to use best score for client

            cluster_id = random.choice(self.scenario['cdn_locations'].keys())
            ad = self.scenario['cdn_locations'][cluster_id]['ad']
            hid = self.scenario['cdn_locations'][cluster_id]['hid']
            name = self.scenario['cdn_locations'][cluster_id]['name']
            logging.debug('no valid bids found using %s (%s %s)' % (name, ad, hid))

        return (name, ad, hid)


    def update_scores(self, msg):
        id = self.getID(msg.client)

        # find or create new client_location entry
        client = self.scenario['client_locations'][id]
        if client == None:
            # FIXME: what do we do about lat/long?
            client = self.make_client_record(msg.client, id, 0.0, 0.0)

        if msg.type == cdn_pb2.PING_SCORES_MSG:
            for cluster in msg.scores.clusters:
                cluster_id = self.getID(cluster.name)
                rtt = cluster.rtt
                loss = cluster.loss

                logging.debug('cluster %s (%d) has %s %s' % (cluster.name, cluster_id, rtt, loss))

                # first pass at making score score = (latency * 1000) + (packet loss percentage * 100)
                # lower is better

                if rtt == None or rtt <= 0 or loss == 100:
                    score = 1000000
                else:
                    score = (rtt * 1000) + (loss * 10000)

                for i, cs in enumerate(client['cluster_scores']):
                    try:
                        if cs[0] == cluster_id:
                            client['cluster_scores'][i][1] = score
                    except:
                        client['cluster_scores'].append([cluster_id, score, 0])

        elif msg.type == cdn_pb2.STATS_SCORE_MSG:
            try:
                client['cluster_stats'] = []

                for cluster in msg.scores.clusters:
                    cluster_id = self.getID(cluster.name)
                    cluster_stats = {}
                    n = 0
                    rate = 0
                    total_bitrate = 0
                    total_cached = 0
                    total_tput = 0.0

                    for stats in cluster.stats:
                        # if we are configured to ignore cached results go on to the next entry
                        if stats.cached and self.allow_cached == False:
                            continue

                        n += 1
                        print stats.bandwidth, stats.tput
                        if stats.cached:
                            total_cached += 1
                        total_bitrate += int(stats.bandwidth)
                        total_tput += float(stats.tput)

                        if stats.tput > client['max_tput']:
                            client['max_tput'] = stats.tput

                    if n > 0:
                        rate = total_bitrate / n
                        tput = total_tput / n
                        cluster_stats['bitrate'] = rate
                        cluster_stats['cached'] = total_cached
                        cluster_stats['tput'] = tput / n

                        client['bitrate'] = rate

                        client['cluster_stats'].append([cluster_id, cluster_stats])

                    for i, cs in enumerate(client['cluster_scores']):
                        try:
                            if cs[0] == cluster_id:
                                client['cluster_scores'][i][2] = tput
                        except:
                            client['cluster_scores'].append([cluster_id, 99999999, rate])

            except Exception as e:
                print str(e)

        logging.debug(client)


# test driver
if __name__ == "__main__":
    f = open(sys.argv[1], 'r')
    s= Scenario(f)
    print s.scenario


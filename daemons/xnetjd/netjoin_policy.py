#! /usr/bin/env python
#

import logging
import hashlib
import networkx
import ndap_pb2
from netjoin_beacon import NetjoinBeacon
from google.protobuf import text_format as protobuf_text_format

class NetjoinPolicy:

    JOINED, JOINING, UNJOINABLE = range(3)

    def __init__(self):
        logging.debug("Policy module initialized")

        # Beacons seen before. One set for each policy instance
        self.known_beacons = {}

        #TODO Load list of available auth providers from config file
        #TODO Load list of XIP networks we can join?

    def print_known_beacons(self):
        print self.known_beacons.keys()

    def keep_known_beacon_id(self, beacon_ID, state):
        if beacon_ID in self.known_beacons:
            logging.debug("Overwriting beacon ID: %s" % beacon_ID)
        self.known_beacons[beacon_ID] = state

    def remove_known_beacon_id(self, beacon_ID):
        if beacon_ID in self.known_beacons:
            del self.known_beacons[beacon_ID]
        else:
            logging.error("Removing non-existing beacon: %s" % beacon_ID)

    # Determine if the beacon has a network descriptor we can deal with
    def sane_beacon(self, beacon):
        # Should have at least two nodes and 1 edge
        if len(beacon.net_descriptor.auth_cap.nodes) < 2:
            return False
        if len(beacon.net_descriptor.auth_cap.edges) < 1:
            return False
        valid_l2_types = []
        valid_l2_types.append(ndap_pb2.LayerTwoIdentifier.ETHERNET)
        valid_l2_types.append(ndap_pb2.LayerTwoIdentifier.WIFI)
        valid_l2_types.append(ndap_pb2.LayerTwoIdentifier.DSRC)
        if beacon.net_descriptor.l2_id.l2_type not in valid_l2_types:
            return False
        return True

    # Remove nodes that we cannot satisfy from the joining graph
    def reduce_graph(self, G):
        # Load list of auth providers we support
        # Load list of XIP networks we can join
        # Check each node except 'Start' against the two lists
        return G

    # Walk a graph and find all XIP nodes
    def find_xip_nodes(self, G):
        xip_nodes = []
        for node in G.nodes():
            if node == 'Start':
                continue
            acnode = ndap_pb2.ACNode()
            acnode.ParseFromString(node)
            #if type(node) is ndap_pb2.XIP_Node:
            if acnode.HasField('xip'):
                xip_nodes.append(node)
        return xip_nodes

    # Get a path of nodes we can satisfy to join an XIP network, or None
    def get_shortest_joinable_path(self, beacon):

        # Sanity check the beacon
        if not self.sane_beacon(beacon):
            logging.error("Bad beacon ignored")
            return False
        # Build a directed graph object we can traverse
        beacon_graph = networkx.DiGraph()

        # List of nodes including a fake Start node
        #nodes_in_order = ['Start'] + beacon.net_descriptor.auth_cap.nodes
        nodes_in_order = ['Start']
        beacon_graph.add_node('Start')
        for node in beacon.net_descriptor.auth_cap.nodes:
            serialized_node = node.SerializeToString()
            nodes_in_order.append(serialized_node)
            beacon_graph.add_node(serialized_node)
        #beacon_graph.add_nodes_from(nodes_in_order)

        # Now add edges by indexing into nodes_in_order
        for edge in beacon.net_descriptor.auth_cap.edges:
            beacon_graph.add_edge(nodes_in_order[edge.from_node], nodes_in_order[edge.to_node])

        # Reduce graph - take out nodes we can't deal with
        beacon_graph = self.reduce_graph(beacon_graph)

        # We will join any available XIP network. Get a list of all XIP nodes
        goal_nodes = self.find_xip_nodes(beacon_graph)

        # Walk the graph to find a path matching available auth providers
        start = nodes_in_order[0]
        path = None
        for xip_node in goal_nodes:
            try:
                path = networkx.shortest_path(beacon_graph, start, xip_node)
                strpath = ["Start"]
                for node in path[1:]:
                    nodemsg = ndap_pb2.ACNode()
                    nodemsg.ParseFromString(node)
                    nodestr = protobuf_text_format.MessageToString(nodemsg)
                    strpath.append(nodestr)
                logging.info("Can join: {}".format(strpath))
                #TODO: find paths to all goal_nodes, try all if first fails
                break
            except networkx.NetworkXNoPath, e:
                pass
        return path

    # Check given beacon ID against list of known beacons
    def is_known_beacon_id(self, beacon_id):
        if beacon_id in self.known_beacons:
            return True
        return False

    # Main entry point for the policy module
    def join_sender_of_serialized_net_descriptor(self, serialized_descriptor):
        join_network = False

        # Convert beacon to an object we can look into
        beacon = NetjoinBeacon()
        beacon.from_serialized_net_descriptor(serialized_descriptor)
        logging.debug("Beacon: {}".format(beacon.beacon_str()))

        # Retrieve ID of beacon so we can test against known beacons
        beacon_ID = beacon.get_ID()

        # If this beacon has been processed before, drop it
        if self.is_known_beacon_id(beacon_ID):
            return False

        # Try to find a set of steps we can take to join a desirable network
        path = self.get_shortest_joinable_path(beacon)
        joining_state = None
        if path:
            # Initiate Netjoiner action and add to known_beacons as JOINING
            logging.debug("Joining: {}".format(beacon.beacon_str()))
            joining_state = self.JOINING
            join_network = True
        else:
            # Add to known_beacons as UNJOINABLE
            joining_state = self.UNJOINABLE
            join_network = False

        # We processed this beacon, so record our decision
        self.keep_known_beacon_id(beacon_ID, joining_state)

        return join_network

# Unit test this module when run by itself
if __name__ == "__main__":
    policy = NetjoinPolicy()
    beacon = NetjoinBeacon()
    beacon.initialize() # A default beacon announcing this network
    serialized_test_beacon = beacon.update_and_get_serialized_beacon()
    test_beacon = ndap_pb2.NetDescriptor()
    test_beacon.ParseFromString(serialized_test_beacon)
    beacon_ID = test_beacon.get_ID()
    policy.keep_known_beacon_id(beacon_ID, NetjoinPolicy.JOINING)
    policy.print_known_beacons()
    policy.remove_known_beacon_id(beacon_ID)
    policy.print_known_beacons()

#! /usr/bin/env python
#

import logging
import binascii
import hashlib
import networkx
import ndap_pb2
import uuid
import os.path
from credmgr import CredMgr
from netjoin_beacon import NetjoinBeacon
from netjoin_xiaconf import NetjoinXIAConf
from netjoin_xrouted import NetjoinXrouted
from netjoin_affinity import NetjoinAffinity
from google.protobuf import text_format as protobuf_text_format

"""Network Joining Policy Module

This module defines the policy for networks that can or should be joined

The join_sender_of_beacon() method is the primary entry point for the
decision making process. Each incoming network announcement beacon is
forwarded to the policy module for a policy decision.

Since each beacon must be processed as it comes in, it is imperative that
the policy decisions are made as efficiently and quickly as possible.

Attributes:
    is_controller: Is the policy module representing a controller?
    is_router: Is the policy module representing a router?
    conf: Interface to read local configuration and process it
    xrouted: Interface to forward relevant messages to the routing daemon
    known_beacons: Beacons for which joining decisions are pre-decided
    affinity: Affinity metric storage for each beacon
    home_xip_network: The Network/AD that this router/controller belongs to

Notes:
    1. home_xip_network is calculated only at __init__. If the credentials
    change at run time, this should also be recalculated.
    2. It might make sense to split NetjoinPolicy to NetjoinControllerPolicy,
    NetjoinRouterPolicy and NetjoinClientPolicy to allow different roles.
    For now, the differences are small so we are just using if-else.
"""
class NetjoinPolicy:

    JOINED, JOINING, UNJOINABLE = range(3)

    def __init__(self, is_controller=False, is_router=False):
        logging.debug("Policy module initialized")
        self.is_controller = is_controller
        self.is_router = is_router
        self.conf = NetjoinXIAConf()
        self.xrouted = NetjoinXrouted()

        # Beacons seen before. One set for each policy instance
        # (beacon_ID, iface) = <current state>
        self.known_beacons = {}

        # Affinity to each beacon on a given interface
        self.affinity = NetjoinAffinity()
        self.affinity.periodic_decrement()

        #TODO Load list of available auth providers from config file
        #TODO Load list of XIP networks we can join?
        # For now, we only join the network in router cred or address.conf
        self.home_xip_network = self.__home_xip_network()

    # Called only once by __init__
    # NOTE may need to make it dynamic if creds can change at runtime
    def __home_xip_network(self):
        """The XIP network that this router/controller belongs to
        NOTE: may be None if the network is not known or running on a client
        """
        home_xip_network = None

        # Controllers have XIP Network ID in etc/address.conf
        if self.is_controller:
            home_xip_network = self.conf.get_raw_ad()
        elif self.is_router:
            credfilepath = os.path.join(self.conf.get_conf_dir(), 'RHID.cred')
            if os.path.exists(credfilepath):
                credmgr = CredMgr()
                router_cred = credmgr.get_router_cred(credfilepath)
                home_xip_network = binascii.unhexlify(router_cred.adonly.ad)

        return home_xip_network

    def print_known_beacons(self):
        logging.info(self.known_beacons.keys())

    def keep_known_beacon_id(self, beacon_ID, iface, state):

        # Invalidate old beacon for the given interface, if one exists
        for key in self.known_beacons.keys():
            identifier, interface = key
            if interface == iface:
                # TODO: decide if we should keep UNJOINABLE beacons
                msg = "Removed beacon:{} iface:{}".format(identifier, interface)
                logging.debug(msg)
                self.remove_known_beacon_id(identifier, interface)

        # Now, keep the new beacon identifier for the interface
        if (beacon_ID, iface) in self.known_beacons:
            logging.debug("Replace beacon:{} iface:{}".format(beacon_ID, iface))
        self.known_beacons[(beacon_ID, iface)] = state

    def remove_known_beacon_id(self, beacon_ID, iface):
        if (beacon_ID, iface) in self.known_beacons:
            del self.known_beacons[(beacon_ID, iface)]
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
    def is_known_beacon_id(self, beacon_id, iface):
        if (beacon_id, iface) in self.known_beacons:
            return True
        return False

    def currently_joined_to(self, interface):
        for ((beacon_id, iface), state) in self.known_beacons.items():
            if iface == interface and state == self.JOINED:
                return beacon_id
        return None

    # Main entry point for the policy module
    def join_sender_of_beacon(self, beacon, iface):
        join_network = False

        # Convert beacon to an object we can look into
        #beacon = NetjoinBeacon()
        #beacon.from_serialized_net_descriptor(serialized_descriptor)
        logging.debug("Beacon: {}".format(beacon.beacon_str()))

        # Retrieve ID of beacon so we can test against known beacons
        beacon_ID = beacon.get_ID()

        # Irrespective of decision on beacon, update its affinity
        self.affinity.update(beacon_ID, iface)

        # Are we currently joined to another network on this interface?
        # TODO: We should do this check after ensuring new beacon is joinable
        old_beacon = self.currently_joined_to(iface)
        if old_beacon:
            oldtuple = (old_beacon, iface)
            newtuple = (beacon_ID, iface)
            if not self.affinity.prefer_over(newtuple, oldtuple):
                return False
            else:
                # TODO: end existing session: it notifies existing router also
                pass

        # If this beacon has been processed before, drop it
        if self.is_known_beacon_id(beacon_ID, iface):
            return False

        # Routers only join network defined by their credential
        # Edge routers notify controller if a foreign network is seen
        # Controller may be an edge router, so check foreign networks on it too
        if self.is_router or self.is_controller:
            # Find the network being announced by the beacon
            beacon_from_network = beacon.find_xip_netid();
            if beacon_from_network == None:
                logging.error("Could not find NetworkId in beacon")
                return False

            # If not our home network, tell xrouted about it and return False
            if self.home_xip_network != beacon_from_network:
                logging.debug("Informing xrouted of a foreign network")
                ad_hex_str = self.conf.raw_ad_to_hex(beacon_from_network)
                beacon_ad_str = "AD:{}".format(ad_hex_str)
                self.xrouted.send_foreign_ad(iface, beacon_ad_str)
                # NOTE add to known beacons to stop notifying xrouted, if needed
                return False

        # Controllers don't join routers
        if self.is_controller:
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
        self.keep_known_beacon_id(beacon_ID, iface, joining_state)

        return join_network

    # Notification that network advertized by beacon_id has been joined
    def join_complete(self, beacon_id, iface):
        if (beacon_id, iface) not in self.known_beacons:
            logging.error("Join complete called for unknown beacon")
            return
        self.known_beacons[(beacon_id, iface)] = self.JOINED
        logging.info("Joined network announced by beacon {}".format(beacon_id))

# Unit test this module when run by itself
if __name__ == "__main__":
    iface = 0
    net_id = uuid.uuid4().bytes
    policy = NetjoinPolicy()
    beacon = NetjoinBeacon()
    beacon.initialize(net_id) # A default beacon announcing this network
    serialized_test_beacon = beacon.update_and_get_serialized_beacon()
    test_beacon = ndap_pb2.NetDescriptor()
    test_beacon.ParseFromString(serialized_test_beacon)
    beacon_ID = test_beacon.get_ID()
    policy.keep_known_beacon_id(beacon_ID, iface, NetjoinPolicy.JOINING)
    policy.print_known_beacons()
    policy.remove_known_beacon_id(beacon_ID, iface)
    policy.print_known_beacons()

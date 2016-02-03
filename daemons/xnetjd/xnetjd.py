#!/usr/bin/python
#

import time
import struct
import socket
import logging
import argparse
import threading
import ndap_pb2
import ndap_beacon
import netjoin_policy

# Setup logging for this application
logging.basicConfig(level=logging.DEBUG, format='%(asctime)s: %(module)s %(levelname)s: %(message)s')

# Announce the presence of this network
class NetjoinAnnouncer(object):
    def announce(self):
        # Send beacon to XIANetJoin
        logging.debug("Sent beacon")
        updated_beacon = self.beacon.update_and_get_serialized_beacon()
        self.sockfd.sendto(updated_beacon, self.xianetjoin)

        # Call ourselves from a new thread after some time
        threading.Timer(self.beacon_interval, self.announce).start()

    def __init__(self, beacon_interval):
        self.beacon_interval = beacon_interval
        self.xianetjoin = ("127.0.0.1", 9882)

        # A socket for sending messages to XIANetJoin
        self.sockfd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        # A beacon object we will send to announce the network
        self.beacon = NetjoinBeacon()


# Listen for incoming beacons
def beacon_handler(policy):
    xnetjd = ("127.0.0.1", 9228)
    sockfd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sockfd.bind(xnetjd)
    while True:
        data, addr = sockfd.recvfrom(1034)
        logging.debug("Received beacon from: {}".format(addr))
        # Convert beacon to protobuf and print its contents
        interface = struct.unpack("H", data[0:2])[0]
        sendermac = struct.unpack("BBBBBB", data[2:8])
        # First two bytes contain interface number
        logging.debug("On interface: {}".format(interface))
        # Six bytes contain source mac address
        logging.debug("Sent by: %02x:%02x:%02x:%02x:%02x:%02x" % sendermac)
        # Remaining data is the beacon
        serialized_beacon = data[8:]
        # Pass the serialized beacon to policy module and move on
        policy.process_serialized_beacon(serialized_beacon)

# Argument parser
def parse_args():
    parser = argparse.ArgumentParser(description="XIA Network Joining Manager")
    parser.add_argument("-c", "--client", help="process network discovery beacons", action="store_true")
    parser.add_argument("-a", "--accesspoint", help="provide access point beacons on all interfaces", action="store_true")
    parser.add_argument("-i", "--beacon_interval", help="beacon interval in seconds(float)", type=float, default=0.5)
    return parser.parse_args()

if __name__ == "__main__":

    # Parse user provided arguments
    args = parse_args()

    # Initialize the policy module
    # For now we have one policy for all beacons.
    # TODO: Pass list of auth providers
    policy = netjoin_policy.NetjoinPolicy()

    if args.accesspoint:
        logging.debug("Announcing network")
        announcer = NetjoinAnnouncer(args.beacon_interval)
        announcer.announce()

    if args.client:
        logging.debug("Listening for network announcements")
        beacon_handler(policy)

#!/usr/bin/python
#

import time
import struct
import socket
import logging
import argparse
import threading
import ndap_pb2
import netjoin_policy
from netjoin_beacon import NetjoinBeacon
from netjoin_receiver import NetjoinReceiver
from netjoin_message_pb2 import NetjoinMessage

# Setup logging for this application
logging.basicConfig(level=logging.INFO, format='%(asctime)s: %(module)s %(levelname)s: %(message)s')

# Announce the presence of this network
class NetjoinAnnouncer(object):

    def announce(self):

        # Exit immediately if shutting down
        if self.shutdown_event.is_set():
            logging.debug("Stopping network announcement")
            return

        # Send beacon to XIANetJoin
        logging.debug("Sent beacon")
        next_serialized_beacon = self.beacon.update_and_get_serialized_beacon()
        beacon_message = NetjoinMessage()
        beacon_message.beacon.ParseFromString(next_serialized_beacon)
        serialized_beacon_message = beacon_message.SerializeToString()

        #self.sockfd.sendto(next_serialized_beacon, self.xianetjoin)
        self.sockfd.sendto(serialized_beacon_message, self.xianetjoin)

        # Call ourselves from a new thread after some time
        threading.Timer(self.beacon_interval, self.announce).start()

    def __init__(self, beacon_interval, shutdown_event):
        self.beacon_interval = beacon_interval
        self.xianetjoin = ("127.0.0.1", 9882)
        self.shutdown_event = shutdown_event

        # A socket for sending messages to XIANetJoin
        self.sockfd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        # A beacon object we will send to announce the network
        self.beacon = NetjoinBeacon()
        self.beacon.initialize()

# Argument parser
def parse_args():
    parser = argparse.ArgumentParser(description="XIA Network Joining Manager")
    parser.add_argument("-c", "--client",
            help="process network discovery beacons", action="store_true")
    parser.add_argument("-a", "--accesspoint",
            help="announce network on all interfaces", action="store_true")
    parser.add_argument("-i", "--beacon_interval",
            help="beacon interval in seconds(float)", type=float, default=0.5)
    parser.add_argument("--hostname",
            help="click hostname", type=str)
    return parser.parse_args()

# Parse arguments and launch necessary threads
def main():

    # Parse user provided arguments
    args = parse_args()

    # All threads must listen for this event and exit gracefully when set
    shutdown_event = threading.Event()

    # List of all threads
    threads = []

    # Initialize the policy module
    # For now we have one policy for all beacons.
    # TODO: Pass list of auth providers
    policy = netjoin_policy.NetjoinPolicy()

    if args.accesspoint:
        logging.debug("Announcing network and listening for join requests")
        announcer = NetjoinAnnouncer(args.beacon_interval, shutdown_event)
        announcer.announce()

    if args.client:
        logging.debug("Listening for network announcements")

    # Start a new thread to listen for messages
    receiver = NetjoinReceiver(policy, shutdown_event)
    receiver.start()
    threads.append(receiver)

    # We block here until the user interrupts or kills the program
    try:
        for thread in threads:
            while thread.is_alive():
                thread.join(timeout=1.0)
    except (KeyboardInterrupt, SystemExit):
        shutdown_event.set()

if __name__ == "__main__":
    main()

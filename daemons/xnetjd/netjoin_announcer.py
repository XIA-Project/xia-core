#!/usr/bin/python
#

import time
import socket
import logging
import threading
from netjoin_beacon import NetjoinBeacon
from netjoin_authsession import NetjoinAuthsession
from netjoin_message_pb2 import NetjoinMessage

# Announce the presence of this network
class NetjoinAnnouncer(object):

    def announce(self):

        # Exit immediately if shutting down
        if self.shutdown_event.is_set():
            logging.debug("Stopping network announcement")
            return

        # Send beacon to XIANetJoin
        logging.debug("Sent beacon")
        net_descriptor = self.beacon.update_and_get_serialized_descriptor()
        beacon_message = NetjoinMessage()
        beacon_message.net_descriptor.ParseFromString(net_descriptor)
        serialized_beacon_message = beacon_message.SerializeToString()
        header = struct.pack("H6B", XNETJ_BROADCAST_IFACE, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff)

        #self.sockfd.sendto(next_serialized_beacon, self.xianetjoin)
        self.sockfd.sendto(header+serialized_beacon_message, self.xianetjoin)

        # Call ourselves from a new thread after some time
        threading.Timer(self.beacon_interval, self.announce).start()

    def __init__(self, beacon_interval, shutdown_event):
        self.beacon_interval = beacon_interval
        self.auth = NetjoinAuthsession()
        self.xianetjoin = ("127.0.0.1", 9882)
        self.shutdown_event = shutdown_event

        # A socket for sending messages to XIANetJoin
        self.sockfd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        # A beacon object we will send to announce the network
        self.beacon = NetjoinBeacon()
        self.beacon.initialize(self.auth.get_raw_verify_key())

# Parse arguments and launch necessary threads
def main():

    # Setup logging for this application
    logging.basicConfig(level=logging.DEBUG, format='%(asctime)s: %(module)s %(levelname)s: %(message)s')

    # Set default parameters for an announcer
    beacon_interval = 1.0
    shutdown_event = threading.Event()

    # Create an annoucner
    announcer = NetjoinAnnouncer(beacon_interval, shutdown_event)
    announcer.announce()

    # Wait for a while
    time.sleep(10)

    # Now trigger the shutdown_event
    shutdown_event.set()

if __name__ == "__main__":
    main()

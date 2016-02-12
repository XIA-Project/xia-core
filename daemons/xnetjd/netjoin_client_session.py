#!/usr/bin/python
#

import time
import socket
import logging
import ndap_pb2
import threading
from netjoin_beacon import NetjoinBeacon
from netjoin_message_pb2 import NetjoinMessage

# A client session reperesenting a client joining a network
class NetjoinClientSession(threading.Thread):
    next_session_ID = 1

    def __init__(self, net_descriptor, shutdown_event):
        threading.Thread.__init__(self)
        self.xianetjoin = ("127.0.0.1", 9882)
        self.beacon = NetjoinBeacon()
        #self.beacon.from_net_descriptor(net_descriptor)
        self.shutdown_event = shutdown_event
        self.session_ID = NetjoinClientSession.next_session_ID
        NetjoinClientSession.next_session_ID += 1

        # A socket for sending messages to XIANetJoin
        self.sockfd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def run(self):
        logging.debug("Started session ID: {}".format(self.session_ID))
        while not self.shutdown_event.is_set():
            time.sleep(0.1)
        logging.debug("Shutting down session ID: {}".format(self.session_ID))


# Parse arguments and launch necessary threads
def main():

    # Setup logging for this application
    logging.basicConfig(level=logging.DEBUG, format='%(asctime)s: %(module)s %(levelname)s: %(message)s')

    # Set default parameters for an announcer
    net_descriptor = ndap_pb2.NetDescriptor()
    shutdown_event = threading.Event()

    # Create a NetjoinClientSession
    session1 = NetjoinClientSession(net_descriptor, shutdown_event)
    session2 = NetjoinClientSession(net_descriptor, shutdown_event)
    session1.start()
    session2.start()

    # Wait for a while
    time.sleep(10)

    # Now trigger the shutdown_event
    shutdown_event.set()

if __name__ == "__main__":
    main()

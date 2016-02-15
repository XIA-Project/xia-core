#!/usr/bin/python
#

import time
import socket
import logging
import ndap_pb2
import threading
import Queue
from netjoin_beacon import NetjoinBeacon
from netjoin_message_pb2 import NetjoinMessage

# A client session reperesenting a client joining a network
class NetjoinSession(threading.Thread):
    next_session_ID = 1

    def __init__(self, net_descriptor, shutdown_event):
        threading.Thread.__init__(self)

        self.xianetjoin = ("127.0.0.1", 9882)
        self.beacon = NetjoinBeacon()
        self.state = self.START
        #self.beacon.from_net_descriptor(net_descriptor)
        self.shutdown_event = shutdown_event
        self.session_ID = NetjoinSession.next_session_ID
        NetjoinSession.next_session_ID += 1

        # A queue for receiving NetjoinMessage messages
        self.q = Queue.Queue()
        self.q.timeout = 0.1

        # A socket for sending messages to XIANetJoin
        self.sockfd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def get_ID(self):
        return self.session_ID()

    # Push a NetjoinMessage to the processing queue from another thread
    def push(self, message):
        self.q.push(message)

    # Send a message out to a specific recipient
    def send_netjoin_message(self, message):
        self.sockfd.sendto(self.xianetjoin, message.SerializeToString())

    def send_handshake_one(self, message):
        # Build handshake one
        # Send handshake one
        self.send_netjoin_message(handshake_one)

    # Main thread handles all messages based on state of joining session
    def run(self):
        logging.debug("Started session ID: {}".format(self.session_ID))

        while not self.shutdown_event.is_set():

            # Keep waiting for a message indefinitely
            try:
                message = self.q.get(block=True, self.q.timeout)
            except (Queue.Empty):
                continue
            # We got a message, determine if we are waiting for it
            message_type = message.WhichOneof("message_type")
            if self.state = self.START:
                if message_type == "beacon":
                    self.state = self.SEND_HS_ONE
                    self.send_handshake_one(message)
                elif message_type == "handshake_one":
                    self.state = self.VALIDATE_HS_ONE
                    solf.got_handshake_one(message)
                else:
                    logging.error("Invalid message: {}".format(message_type))
                    break

        logging.debug("Shutting down session ID: {}".format(self.session_ID))


# Parse arguments and launch necessary threads
def main():

    # Setup logging for this application
    logging.basicConfig(level=logging.DEBUG, format='%(asctime)s: %(module)s %(levelname)s: %(message)s')

    # Set default parameters for an announcer
    net_descriptor = ndap_pb2.NetDescriptor()
    shutdown_event = threading.Event()

    # Create a NetjoinSession
    session1 = NetjoinSession(net_descriptor, shutdown_event)
    session2 = NetjoinSession(net_descriptor, shutdown_event)
    session1.start()
    session2.start()

    # Wait for a while
    time.sleep(10)

    # Now trigger the shutdown_event
    shutdown_event.set()

if __name__ == "__main__":
    main()

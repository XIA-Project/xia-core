#! /usr/bin/env python
#

import socket
import struct
import logging
import threading
from netjoin_policy import NetjoinPolicy
from netjoin_message_pb2 import NetjoinMessage

# Receiver thread, only responsible for receiving netjoin related packets
# from the XIANetJoin click module and invoking the handler for each packet
class NetjoinReceiver(threading.Thread):

    def __init__(self, policy, shutdown, xnetjd_addr=("127.0.0.1", 9228)):
        threading.Thread.__init__(self)
        self.BUFSIZE = 1024
        self.sockfd = None
        self.shutdown = shutdown
        self.policy = policy
        self.client_sessions = {}

        logging.debug("Receiver initialized")

        # A socket we will bind to and listen on
        self.sockfd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sockfd.bind(xnetjd_addr)

    def handle_beacon(self, message):

        logging.debug("Got a beacon")
        serialized_beacon = message.beacon.SerializeToString()

        # Ask the policy module if we should join this network
        if not self.policy.join_sender_of_serialized_beacon(
                serialized_beacon):
            logging.debug("Policy action: ignore beacon")
            return

        # Initiate session to join the network
        session = NetjoinSession(beacon, self.shutdown)
        session.daemon = True
        session.start()
        self.client_sessions[session.get_ID()] = session

        # Give the NetjoinMessage containing beacon to NetjoinSession
        session.push(message)

    def handle_handshake_one(self):
        logging.debug("Got HandshakeOne message")
        # Retrieve session ID from handshake one
        # Pass handshake one to the corresponding session handler
        pass

    # Main loop, receives incoming NetjoinMessage(s)
    def run(self):
        while not self.shutdown.is_set():
            data, addr = self.sockfd.recvfrom(self.BUFSIZE)

            # Retrieve incoming interface and sender's mac address
            interface = struct.unpack("H", data[0:2])[0]
            sendermac = struct.unpack("BBBBBB", data[2:8])

            # First two bytes contain interface number
            logging.debug("On interface: {}".format(interface))

            # Six bytes contain source mac address
            logging.debug("Sent by: %02x:%02x:%02x:%02x:%02x:%02x" % sendermac)

            # Remaining data is the beacon
            serialized_message = data[8:]

            # Identify what kind of message this is
            netjoin_message = NetjoinMessage()
            netjoin_message.ParseFromString(serialized_message)
            message_type = netjoin_message.WhichOneof("message_type")
            logging.debug("Message type: {}".format(message_type))

            # A new session is started if client decides to join a network
            # or if an access point receives a handshake one reqest to join
            if message_type == "beacon":
                self.handle_beacon(netjoin_message)

            elif message_type == "handshake_one":
                self.handle_handshake_one()

            else:
                logging.warning("Unknown message type: {}".format(message_type)

# Unit test this module when run by itself
if __name__ == "__main__":

    logging.basicConfig(level=logging.DEBUG,
            format='%(asctime)s: %(module)s %(levelname)s: %(message)s')

    policy = NetjoinPolicy()
    shutdown_event = threading.Event()
    receiver = NetjoinReceiver(policy, shutdown_event)
    receiver.start()
    try:
        while receiver.is_alive():
            receiver.join(timeout=1.0)
    except (KeyboardInterrupt, SystemExit):
        shutdown_event.set()

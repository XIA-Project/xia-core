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

        logging.debug("Receiver initialized")

        # A socket we will bind to and listen on
        self.sockfd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sockfd.bind(xnetjd_addr)

    def run(self):
        while not self.shutdown.is_set():
            data, addr = self.sockfd.recvfrom(self.BUFSIZE)

            # Convert beacon to protobuf and print its contents
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

            if message_type == "beacon":
                logging.debug("Got a beacon")
                self.policy.process_serialized_beacon(netjoin_message.beacon.SerializeToString())
            elif message_type == "handshake_one":
                logging.debug("Got handshake_one message")

# Unit test this module when run by itself
if __name__ == "__main__":

    logging.basicConfig(level=logging.DEBUG, format='%(asctime)s: %(module)s %(levelname)s: %(message)s')

    policy = NetjoinPolicy()
    shutdown_event = threading.Event()
    receiver = NetjoinReceiver(policy, shutdown_event)
    receiver.start()
    try:
        while receiver.is_alive():
            receiver.join(timeout=1.0)
    except (KeyboardInterrupt, SystemExit):
        shutdown_event.set()

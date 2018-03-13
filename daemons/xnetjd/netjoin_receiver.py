#! /usr/bin/env python
#

import copy
import socket
import struct
import logging
import threading
from netjoin_policy import NetjoinBeacon
from netjoin_policy import NetjoinPolicy
from netjoin_session import NetjoinSession
from netjoin_announcer import NetjoinAnnouncer
from netjoin_message_pb2 import NetjoinMessage

# Receiver thread, only responsible for receiving netjoin related packets
# from the XIANetJoin click module and invoking the handler for each packet
class NetjoinReceiver(threading.Thread):

    def __init__(self, hostname, policy, announcer, shutdown,
            xnetjd_addr=("127.0.0.1", 9228), is_router=False):
        threading.Thread.__init__(self)
        self.BUFSIZE = 1024
        self.sockfd = None
        self.shutdown = shutdown
        self.hostname = hostname
        self.policy = policy
        self.announcer = announcer    # None, unless running as access point
        self.is_router = is_router
        self.client_sessions = {}
        self.server_sessions = {}
        self.existing_sessions = {}
        self.cleanup_sessions = []

        logging.debug("Receiver initialized")

        # A socket we will bind to and listen on
        self.sockfd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sockfd.bind(xnetjd_addr)

    def announce(self, l2_type, net_id, beacon_interval):

        if self.announcer != None:
            self.announcer.stop()

	self.announcer = NetjoinAnnouncer(l2_type, net_id, beacon_interval,
		self.shutdown)
	self.announcer.announce()

    def get_net_id(self):
        return self.announcer.get_net_id()

    def handle_net_descriptor(self, message_tuple):

        logging.debug("Got a beacon")

        message = message_tuple[0]
        iface = message_tuple[1]
        # Convert network descriptor to a beacon
        serialized_net_descriptor = message.net_descriptor.SerializeToString()
        beacon = NetjoinBeacon()
        beacon.from_serialized_net_descriptor(serialized_net_descriptor)

        # Ask the policy module if we should join this network
        if not self.policy.join_sender_of_beacon(beacon, iface):
            logging.debug("Policy action: ignore beacon")
            return

        # Initiate a NetjoinSession thread to join the network
        session = NetjoinSession(self.hostname, self.shutdown,
                receiver=self,
                is_router=self.is_router,
                beacon_id=beacon.get_ID(), policy=self.policy)
        session.daemon = True
        session.start()
        self.client_sessions[session.get_ID()] = session

        # Send the message info to the NetjoinSession
        session.push(message_tuple)

    # First message received from client when running as access point
    def handle_handshake_one(self, message_tuple):
        logging.info("Got HandshakeOne message")

        # Do not create more than one session for a client
        # unless the previous session was complete (rejoining client)
        netjoin_msg = message_tuple[0]
        iface = message_tuple[1]
        client_key = netjoin_msg.handshake_one.encrypted.client_ephemeral_pubkey
        identifier = (client_key, iface)
        if identifier in self.existing_sessions:
            # TODO: It might be possible to drop all HS1 from existing sessions
            logging.info("A session with this client already exists")
            session_state = self.existing_sessions[identifier].state
            if session_state == NetjoinSession.HS_3_WAIT:
                self.existing_sessions[identifier].push(message_tuple)
                return

            if session_state != NetjoinSession.HS_DONE:
                logging.info("Session not in DONE state, skip this HS1")
                return

        # TODO: Avoid copying announcer's auth because that copies private key
        # Copy the auth session from announcer to bootstrap a new session
        authsession = copy.copy(self.announcer.auth)
        session = NetjoinSession(self.hostname, self.shutdown,
                auth=authsession, receiver=self)
        session.daemon = True
        session.start()

        # TODO: Retrieve session ID from handshake one
        self.server_sessions[session.get_ID()] = session

        # This overwrites any session that was in DONE state
        # TODO: entries in existing_sessions need to be cleared after join
        self.existing_sessions[identifier] = session

        # Pass handshake one to the corresponding session handler
        session.push(message_tuple)

    def handle_handshake_two(self, message_tuple):
        logging.info("Got HandshakeTwo message")

        # Find the session this message should be delivered to
        handshake_two = message_tuple[0].handshake_two
        client_session_id = handshake_two.client_session_id

        # Deliver the message
        if not client_session_id in self.client_sessions:
            logging.error("Session not found to deliver handshake two")
            return
        client_session = self.client_sessions[client_session_id]
        client_session.push(message_tuple)

    def handle_handshake_three(self, message_tuple):
        logging.info("Got HandshakeThree message")

        # Find the session this message should be delivered to
        handshake_three = message_tuple[0].handshake_three
        gateway_session_id = handshake_three.gateway_session_id

        # Deliver the message
        if not gateway_session_id in self.server_sessions:
            logging.error("Session not found to deliver handshake three")
            return
        gateway_session = self.server_sessions[gateway_session_id]
        gateway_session.push(message_tuple)

    def handle_handshake_four(self, message_tuple):
        logging.info("Got HandshakeFour message")

        # Find the session this message should be delivered to
        handshake_four = message_tuple[0].handshake_four
        client_session_id = handshake_four.client_session_id

        # Deliver the message
        if not client_session_id in self.client_sessions:
            logging.error("Session not found to deliver handshake four")
            return
        client_session = self.client_sessions[client_session_id]
        client_session.push(message_tuple)

    def session_done(self, session_id):
        # Find the session and remove it from client/server_sessions
        if session_id in self.client_sessions:
            session = self.client_sessions[session_id]
            del self.client_sessions[session_id]
        elif session_id in self.server_sessions:
            session = self.server_sessions[session_id]
            del self.server_sessions[session_id]
        else:
            logging.error("Completed session not known to receiver")
            return
        self.cleanup_sessions.append(session)

    # Main loop, receives incoming NetjoinMessage(s)
    def run(self):
        while not self.shutdown.is_set():
            data, addr = self.sockfd.recvfrom(self.BUFSIZE)

            # Retrieve incoming interface and sender's mac address
            interface = struct.unpack("H", data[0:2])[0]
            mymac = struct.unpack("BBBBBB", data[2:8])
            sendermac = struct.unpack("BBBBBB", data[8:14])

            # First two bytes contain interface number
            logging.debug("On interface: {}".format(interface))

            # Six bytes contain source mac address
            logging.debug("Sent by: %02x:%02x:%02x:%02x:%02x:%02x" % sendermac)
            logging.debug("Sent to: %02x:%02x:%02x:%02x:%02x:%02x" % mymac)

            # Remaining data is the beacon
            serialized_message = data[14:]

            # Identify what kind of message this is
            netjoin_message = NetjoinMessage()
            netjoin_message.ParseFromString(serialized_message)
            message_type = netjoin_message.WhichOneof("message_type")
            logging.debug("Message type: {}".format(message_type))

            # Collect all info about this message into a message_tuple
            message_tuple = (netjoin_message, interface, mymac, sendermac)

            # A new session is started if client decides to join a network
            # or if an access point receives a handshake one reqest to join
            if message_type == "net_descriptor":
                self.handle_net_descriptor(message_tuple)

            elif message_type == "handshake_one":
                self.handle_handshake_one(message_tuple)

            elif message_type == "handshake_two":
                self.handle_handshake_two(message_tuple)

            elif message_type == "handshake_three":
                self.handle_handshake_three(message_tuple)

            elif message_type == "handshake_four":
                self.handle_handshake_four(message_tuple)

            else:
                logging.warning("Unknown msg type: {}".format(message_type))

            # Join any exited session threads
            if len(self.cleanup_sessions) > 0:
                session = self.cleanup_sessions.pop()
                if session.is_alive():
                    session.join(timeout=1.0)

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

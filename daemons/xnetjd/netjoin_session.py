#!/usr/bin/python
#

import time
import socket
import struct
import logging
import threading
import Queue
import c_xsocket
from clickcontrol import ClickControl
from ndap_pb2 import LayerTwoIdentifier, NetDescriptor
from netjoin_beacon import NetjoinBeacon
from netjoin_message_pb2 import NetjoinMessage
from netjoin_authsession import NetjoinAuthsession
from netjoin_ethernet_handler import NetjoinEthernetHandler
from netjoin_handshake_one import NetjoinHandshakeOne
from netjoin_handshake_two import NetjoinHandshakeTwo
from netjoin_handshake_three import NetjoinHandshakeThree

# A client session reperesenting a client joining a network
class NetjoinSession(threading.Thread):
    next_session_ID = 1

    # auth is set on gateway side, new one created on client for handshake 1
    # beacon_id is set only on client side
    # policy is set only on client side
    def __init__(self, hostname, shutdown_event,
            auth=None, beacon_id=None, policy=None):
        threading.Thread.__init__(self)
        (self.START, self.HS_2_WAIT, self.HS_3_WAIT) = range(3)

        self.xianetjoin = ("127.0.0.1", 9882)
        self.beacon = NetjoinBeacon()
        self.state = self.START
        self.hostname = hostname
        self.shutdown_event = shutdown_event
        self.auth = auth
        self.beacon_id = beacon_id
        self.policy = policy
        self.l2_handler = None    # Created on receiving beacon or handshake1
        self.session_ID = NetjoinSession.next_session_ID
        NetjoinSession.next_session_ID += 1
        # TODO Is this the best place to store client HID recv'd in H1?
        self.client_hid = None

        # Create an encryption/authentication session if one not provided
        if not self.auth:
            self.auth = NetjoinAuthsession()

        # A queue for receiving NetjoinMessage messages
        self.q = Queue.Queue()
        self.q.timeout = 0.1

        # A socket for sending messages to XIANetJoin
        self.sockfd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def get_ID(self):
        return self.session_ID

    # Push a NetjoinMessage to the processing queue from another thread
    def push(self, message_tuple):
        self.q.put(message_tuple)

    # Send a message out to a specific recipient
    def send_netjoin_message(self, message, interface, theirmac):
        netj_header = struct.pack("H6B", interface, theirmac[0], theirmac[1],
                theirmac[2], theirmac[3], theirmac[4], theirmac[5])
        outgoing_packet = netj_header + message.SerializeToString()
        self.sockfd.sendto(outgoing_packet, self.xianetjoin)

    def create_l2_handler(self, l2_type):
        l2_handler = None

        if l2_type == LayerTwoIdentifier.ETHERNET:
            l2_handler = NetjoinEthernetHandler()
        else:
            logging.error("Invalid l2_type: {}".format(l2_type))

        return l2_handler

    def handle_net_descriptor(self, message_tuple):
        message, interface, mymac, theirmac = message_tuple

        logging.info("Got a beacon from a network we want to join")

        # A layer 2 handler for creating/processing l2 requests
        l2_type = message.net_descriptor.l2_id.l2_type
        self.l2_handler = self.create_l2_handler(l2_type)

        # Save access point verify key included in NetDescriptor message
        join_auth_info = message.net_descriptor.ac_shared.ja
        gateway_raw_key = join_auth_info.gateway_ephemeral_pubkey.the_key
        self.auth.set_their_raw_verify_key(gateway_raw_key)

        # Retrieve gateway_nonce as the challenge to include in l3 request
        gw_nonce = join_auth_info.gateway_nonce

        # Build handshake one
        netjoin_h1 = NetjoinHandshakeOne(self, mymac, challenge=gw_nonce)
        # Nonce MUST be updated every time before sending out
        netjoin_h1.update_nonce()

        outgoing_message = NetjoinMessage()
        outgoing_message.handshake_one.CopyFrom(netjoin_h1.handshake_one)

        # Send handshake one
        self.send_netjoin_message(outgoing_message, interface, theirmac)

        # Now we will wait for handshake two
        self.state = self.HS_2_WAIT

    def handle_handshake_one(self, message_tuple):
        message, interface, mymac, theirmac = message_tuple

        # Convert incoming protobuf to a handshake one object we can assess
        netjoin_h1 = NetjoinHandshakeOne(self, mymac)
        netjoin_h1.from_wire_handshake_one(message.handshake_one)
        netjoin_h1.print_handshake_one()
        netjoin_h1.print_payload()

        # Layer 2 handler to create/process l2 requests
        self.l2_handler = self.create_l2_handler(netjoin_h1.l2_type())

        # Validate incoming client joining request (handshake one)
        deny_h2 = False
        l2_request = netjoin_h1.payload.core.client_l2_req
        l2_reply = self.l2_handler.handle_request(l2_request)
        if not netjoin_h1.is_valid():
            deny_h2 = True
            logging.info("HandshakeOne invalid, denying connection request")
        logging.info("Accepted handshake one from client")
        self.client_hid = netjoin_h1.hex_client_hid()

        # Retrieve handshake one info to be included in handshake two
        client_session_id = netjoin_h1.client_session_id()

        # Now build a handshake two message
        logging.info("Now sending handshake two")
        netjoin_h2 = NetjoinHandshakeTwo(self, deny=deny_h2,
                client_session=client_session_id, l2_reply=l2_reply)
        netjoin_h2.update_nonce()

        # Package the handshake two into a netjoin message wrapper
        outgoing_message = NetjoinMessage()
        outgoing_message.handshake_two.CopyFrom(netjoin_h2.handshake_two)
        self.send_netjoin_message(outgoing_message, interface, theirmac)

        # Now we will wait for handshake 3
        self.state = self.HS_3_WAIT

    def handle_handshake_two(self, message_tuple):
        message, interface, mymac, theirmac = message_tuple

        logging.info("Got a handshake two message")
        netjoin_h2 = NetjoinHandshakeTwo(self)
        netjoin_h2.from_wire_handshake_two(message.handshake_two)
        netjoin_h2.print_handshake_two()
        netjoin_h2.print_cyphertext()

        # Verify the client_session_id matches the encrypted one
        if not netjoin_h2.valid_client_session_id():
            logging.error("Invalid client session ID")
            return

        # Ensure that our netjoin request was granted
        if not netjoin_h2.join_granted():
            logging.error("Our join request was denied")
            return
        logging.info("Valid handshake two: We can join this network now")

        # Configure Click info
        router_dag = str(netjoin_h2.router_dag())
        router_4id = ""
        logging.info("Router DAG is: {}".format(router_dag))
        sockfd = c_xsocket.Xsocket(c_xsocket.SOCK_STREAM)
        retval = c_xsocket.XupdateDAG(sockfd, interface, router_dag, router_4id)
        if retval != 0:
            logging.error("Failed updating DAG in XIA stack")
        logging.info("Local DAG updated")

        # Set the nameserver dag into Click
        ns_dag = str(netjoin_h2.nameserver_dag())
        with ClickControl() as click:
            if click.setNSDAG(ns_dag) == False:
                logging.error("Failed updating Nameserver DAG in XIA Stack")
        logging.info("Nameserver DAG updated")

        # Retrieve handshake two info to be included in handshake three
        gateway_session_id = netjoin_h2.get_gateway_session_id()

        # Build a handshake three in responso to this handshake two
        logging.info("Sending handshake three")
        netjoin_h3 = NetjoinHandshakeThree(self, deny=False,
                gateway_session=gateway_session_id)
        netjoin_h3.update_nonce()

        # Package the handshake three into a netjoin message wrapper
        outgoing_message = NetjoinMessage()
        outgoing_message.handshake_three.CopyFrom(netjoin_h3.handshake_three)
        self.send_netjoin_message(outgoing_message, interface, theirmac)

        # notify policy module that the client side handshake is complete
        if self.policy is None:
            logging.error("Policy module not known for joined network")
            return
        self.policy.join_complete(self.beacon_id)

    def handle_handshake_three(self, message_tuple):
        message, interface, mymac, theirmac = message_tuple

        logging.info("Got a handshake three message")
        netjoin_h3 = NetjoinHandshakeThree(self)
        netjoin_h3.from_wire_handshake_three(message.handshake_three)
        netjoin_h3.print_handshake_three()
        netjoin_h3.print_cyphertext()

        # Verify the client_session_id matches the encrypted one
        if not netjoin_h3.valid_gateway_session_id():
            logging.error("Invalid client session ID")
            return

        # Ensure that our netjoin request was granted
        if not netjoin_h3.join_granted():
            logging.error("Our join request was denied")
            return
        logging.info("Valid handshake three: We can join this network now")

        # Configure Click info
        if len(self.client_hid) < 20:
            logging.error("Client HID not known while handling H3")
            return
        client_hid = str("HID:{}".format(self.client_hid))
        logging.info("Routing table now points to: {}".format(client_hid))
        # TODO: Configure routing table
        # interface number is the port for the route
        # client_hid is the entry in HID routing table and also next hop
        # flags are 0xffff
        flags = 0xffff
        with ClickControl() as click:
            ret = click.setHIDRoute(self.hostname, client_hid, interface, flags)
            if ret == False:
                logging.error("Failed setting route for {}".format(client_hid))
            else:
                logging.info("Route set up for {}".format(client_hid))

    # Main thread handles all messages based on state of joining session
    def run(self):
        logging.debug("Started session ID: {}".format(self.session_ID))

        while not self.shutdown_event.is_set():

            # Keep waiting for a message indefinitely
            try:
                message_tuple = self.q.get(block=True, timeout=self.q.timeout)
                message = message_tuple[0]
            except (Queue.Empty):
                # TODO: Retransmit last message if not in START state
                continue

            # We got a message, determine if we are waiting for it
            message_type = message.WhichOneof("message_type")
            logging.info("Got a {} message".format(message_type))
            if self.state == self.START:
                if message_type == "net_descriptor":
                    self.handle_net_descriptor(message_tuple)
                elif message_type == "handshake_one":
                    self.handle_handshake_one(message_tuple)
                else:
                    logging.error("Invalid message: {}".format(message_type))
                    logging.error("Expected net_descriptor or handshake one")
                continue

            if self.state == self.HS_2_WAIT:
                if message_type == "handshake_two":
                    self.handle_handshake_two(message_tuple)
                else:
                    logging.error("Invalid message: {}".format(message_type))
                    logging.error("Expected handshake two.")
                continue

            if self.state == self.HS_3_WAIT:
                if message_type == "handshake_three":
                    self.handle_handshake_three(message_tuple)
                else:
                    logging.error("Invalid message: {}".format(message_type))
                    logging.error("Expected handshake three.")
                continue

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

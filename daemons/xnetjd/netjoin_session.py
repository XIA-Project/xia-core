#!/usr/bin/python
#

import time
import socket
import struct
import logging
import threading
import Queue
import dagaddr
import c_xsocket
import socket
from clickcontrol import ClickControl
from ndap_pb2 import LayerTwoIdentifier, NetDescriptor
from dagaddr import Graph
from netjoin_beacon import NetjoinBeacon
from netjoin_message_pb2 import NetjoinMessage
from netjoin_authsession import NetjoinAuthsession
from netjoin_xiaconf import NetjoinXIAConf
from netjoin_xrouted import NetjoinXrouted
from netjoin_dsrc_handler import NetjoinDSRCHandler
from netjoin_ethernet_handler import NetjoinEthernetHandler
from netjoin_handshake_one import NetjoinHandshakeOne
from netjoin_handshake_two import NetjoinHandshakeTwo
from netjoin_handshake_three import NetjoinHandshakeThree
from netjoin_handshake_four import NetjoinHandshakeFour
import xroute_pb2

# A client session reperesenting a client joining a network
class NetjoinSession(threading.Thread):
    next_session_ID = 1
    (START,
                BEACON_RCVD, HS_1_RCVD,
                HS_2_WAIT, HS_2_RCVD,
                HS_3_WAIT, HS_3_RCVD,
                HS_4_WAIT, HS_4_RCVD,
                HS_DONE) = range(10)

    # auth is set on gateway side, new one created on client for handshake 1
    # beacon_id is set only on client side
    # policy is set only on client side
    def __init__(self, hostname, shutdown_event,
            receiver=None,
            is_router=False,
            auth=None, beacon_id=None, policy=None):
        threading.Thread.__init__(self)

        self.xianetjoin = ("127.0.0.1", 9882)
        self.conf = NetjoinXIAConf()
        self.beacon = NetjoinBeacon()
        self.state = self.START
        self.start_time = time.time()
        self.hostname = hostname
        self.shutdown_event = shutdown_event
        self.receiver = receiver
        self.auth = auth
        self.beacon_id = beacon_id
        self.policy = policy
        self.xrouted = NetjoinXrouted()
        self.is_router = is_router
        self.controller_dag = None

        self.router_sid = None
        self.router_hid = None

        self.l2_handler = None    # Created on receiving beacon or handshake1
        self.last_message_tuple = None # (message, outgoing_interface, theirmac)
        self.last_message_remaining_iterations = None
        self.retransmit_iterations = None
        self.session_ID = NetjoinSession.next_session_ID
        NetjoinSession.next_session_ID += 1
        # TODO Is this the best place to store client HID recv'd in H1?
        self.client_hid = None

        # Create an encryption/authentication session if one not provided
        if not self.auth:
            self.auth = NetjoinAuthsession()

        # A queue for receiving NetjoinMessage messages
        self.q = Queue.Queue()
        self.q.timeout = 0.1      # Changes to l2_handler.rate()

        # A socket for sending messages to XIANetJoin
        self.sockfd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def get_ID(self):
        return self.session_ID

    # Push a NetjoinMessage to the processing queue from another thread
    def push(self, message_tuple):
        self.q.put(message_tuple)

    # Send a message out to a specific recipient, return True on success
    def send_netjoin_message(self, message, interface, theirmac,
            retransmitting=False):

        msgtype = type(message)

        # If we are retransmitting a message, decrement remaining iterations
        if retransmitting:
            self.last_message_remaining_iterations -= 1
            if self.last_message_remaining_iterations <= 0:
                self.disable_retransmission()
                logging.error("retransmit {} timeout".format(msgtype))
                return False
            else:
                logging.info("retransmitting {}".format(msgtype))
        else:
            # Otherwise, allow a new set of iterations for a new message
            logging.info("sending {} with {} retries".format(msgtype,
                self.retransmit_iterations))
            self.last_message_remaining_iterations = self.retransmit_iterations

        # Update nonce every time we send a message
        message.update_nonce()

        # Build a NetjoinMessage object containing the actual handshake message
        netj_msg = NetjoinMessage()
        if type(message) is NetjoinHandshakeOne:
            netj_msg.handshake_one.CopyFrom(message.handshake_one)
        elif type(message) is NetjoinHandshakeTwo:
            netj_msg.handshake_two.CopyFrom(message.handshake_two)
        elif type(message) is NetjoinHandshakeThree:
            netj_msg.handshake_three.CopyFrom(message.handshake_three)
        elif type(message) is NetjoinHandshakeFour:
            netj_msg.handshake_four.CopyFrom(message.handshake_four)
        else:
            logging.error("{} message not supported".format(type(message)))
            return False

        # Header contains outgoing interface and destination mac address
        netj_header = struct.pack("H6B", interface, theirmac[0], theirmac[1],
                theirmac[2], theirmac[3], theirmac[4], theirmac[5])

        # Final outgoing packet with header and serialized NetjoinMessage
        outgoing_packet = netj_header + netj_msg.SerializeToString()
        self.sockfd.sendto(outgoing_packet, self.xianetjoin)

        # Remember this message in case we need to resend it
        self.last_message_tuple = (message, interface, theirmac)

        return True

    def create_l2_handler(self, l2_type):
        l2_handler = None

        if l2_type == LayerTwoIdentifier.ETHERNET:
            l2_handler = NetjoinEthernetHandler()
        elif l2_type == LayerTwoIdentifier.DSRC:
            l2_handler = NetjoinDSRCHandler()
        else:
            logging.error("Invalid l2_type: {}".format(l2_type))

        # Set the retransmission iterations and timeout
        self.retransmit_iterations = l2_handler.iterations()
        self.q.timeout = l2_handler.rate()

        return l2_handler

    def handle_net_descriptor(self, message_tuple):
        message, interface, mymac, theirmac = message_tuple

        self.state = self.BEACON_RCVD

        logging.info("Got a beacon from a network we want to join")

        # A layer 2 handler for creating/processing l2 requests
        l2_type = message.net_descriptor.l2_id.l2_type
        self.l2_handler = self.create_l2_handler(l2_type)

        # Save the network descriptor (beacon)
        self.beacon.from_net_descriptor(message.net_descriptor)

        # Save access point verify key included in NetDescriptor message
        join_auth_info = message.net_descriptor.ac_shared.ja
        gateway_raw_key = join_auth_info.gateway_ephemeral_pubkey.the_key
        self.auth.set_their_raw_verify_key(gateway_raw_key)

        # Retrieve gateway_nonce as the challenge to include in l3 request
        gw_nonce = join_auth_info.gateway_nonce

        # Build handshake one
        netjoin_h1 = NetjoinHandshakeOne(self, mymac,
                is_router=self.is_router, challenge=gw_nonce)

        # Send handshake one
        self.send_netjoin_message(netjoin_h1, interface, theirmac)

        # Now we will wait for handshake two
        self.state = self.HS_2_WAIT

    def handle_handshake_one(self, message_tuple):
        message, interface, mymac, theirmac = message_tuple

        self.state = self.HS_1_RCVD

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
        client_is_router = netjoin_h1.is_from_router()

        # Retrieve handshake one info to be included in handshake two
        client_session_id = netjoin_h1.client_session_id()

        # Now build a handshake two message
        logging.info("Now sending handshake two")
        netjoin_h2 = NetjoinHandshakeTwo(self, deny=deny_h2,
                hostname=self.hostname,
                client_session=client_session_id, l2_reply=l2_reply,
                client_is_router=client_is_router)

        # Send handshake two
        self.send_netjoin_message(netjoin_h2, interface, theirmac)

        # We don't need to retransmit handshake 2
        self.disable_retransmission()

        # Now we will wait for handshake 3
        self.state = self.HS_3_WAIT

    def handle_handshake_two(self, message_tuple):
        message, interface, mymac, theirmac = message_tuple

        self.disable_retransmission()
        self.state = self.HS_2_RCVD

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

        # Save controller_dag for xrouted notification after hs4
        if self.is_router:
            self.controller_dag = str(netjoin_h2.controller_dag())

        # Get router's SID for xrouted notification after hs4
        self.router_sid = str(netjoin_h2.router_sid())

        # Configure Click info
        router_dag = str(netjoin_h2.router_dag())
        router_4id = ""
        logging.info("Router DAG is: {}".format(router_dag))
        sockfd = c_xsocket.Xsocket(c_xsocket.SOCK_DGRAM)
        retval = c_xsocket.XupdateDAG(sockfd, interface, router_dag,
                router_4id, self.is_router)
        c_xsocket.Xclose(sockfd)
        if retval != 0:
            logging.error("Failed updating DAG in XIA stack")
        logging.info("Local DAG updated")

        # Click configuration
        with ClickControl() as click:

            # Set the nameserver dag into Click
            ns_dag = netjoin_h2.nameserver_dag()
            if click.setNSDAG(str(ns_dag)) == False:
                logging.error("Failed updating Nameserver DAG in XIA Stack")
            logging.info("Nameserver DAG updated")

            # If router, add AD into the XARP Table
            if self.is_router:
                ad = Graph(router_dag).intent_AD_str()
                if len(ad) < 40:
                    logging.error("Intent AD not found")
                    return
                if click.setADInXARPTable(ad) == False:
                    logging.error("Failed adding {} to XARP".format(ad))
                    return
                logging.info("Added {} to XARP tables".format(ad))

            # Set the Rendezvous DAG into Click
            router_rv_dag = netjoin_h2.router_rv_dag()
            if router_rv_dag:
                # TODO: Switch out router HID with client HID
                if click.assignRVDAG(self.hostname,
                        str(router_rv_dag), interface) == False:
                    logging.error("Failed updating RV DAG in XIA Stack")
                    return
                logging.info("Router DAG sent to Click and processed")

            # TODO: Set the Rendezvous Control DAG into Click
            control_rv_dag = netjoin_h2.control_rv_dag()
            if control_rv_dag:
                if click.assignRVControlDAG(self.hostname, "XIAEndHost",
                        str(control_rv_dag), interface)==False:
                    logging.error("Failed updating Control RV DAG in XIA Stack")
                    return
                logging.info("Control RV DAG sent to Click and processed")

        # TODO: Create XARP entry for router HID
        self.router_hid = Graph(router_dag).intent_HID_str()
        sendermacstr = "%02x:%02x:%02x:%02x:%02x:%02x" % (theirmac)
        logging.info("XARP: {} has {}".format(sendermacstr, self.router_hid))

        # Inform RV service of new network joined
        # TODO: This should really happen on receiving handshake four
        sockfd = c_xsocket.Xsocket(c_xsocket.SOCK_DGRAM)
        retval = c_xsocket.XupdateRV(sockfd, interface);
        c_xsocket.Xclose(sockfd)
        if retval != 0:
            logging.error("Failed notifying RV service of new location")
        logging.info("Rendezvous service notified")

        # Retrieve handshake two info to be included in handshake three
        gateway_session_id = netjoin_h2.get_gateway_session_id()

        # Build a handshake three in responso to this handshake two
        logging.info("Sending handshake three")
        netjoin_h3 = NetjoinHandshakeThree(self, deny=False,
                gateway_session=gateway_session_id)

        # Send handshake three
        self.send_netjoin_message(netjoin_h3, interface, theirmac)

        # notify policy module that the client side handshake is complete
        if self.policy is None:
            logging.error("Policy module not known for joined network")
            return
        self.policy.join_complete(self.beacon_id, interface)

        # Now we are waiting for handshake 4 confirmation message
        self.state = self.HS_4_WAIT

    def handle_handshake_three(self, message_tuple):
        message, interface, mymac, theirmac = message_tuple

        self.state = self.HS_3_RCVD

        # Disable retransmission of handshake two
        self.disable_retransmission()

        # Now handle the handshake 3 received over the wire
        logging.info("Got a handshake three message")
        netjoin_h3 = NetjoinHandshakeThree(self)
        netjoin_h3.from_wire_handshake_three(message.handshake_three)
        netjoin_h3.print_handshake_three()
        netjoin_h3.print_cyphertext()

        # Verify the client_session_id matches the encrypted one
        if not netjoin_h3.valid_gateway_session_id():
            logging.error("Invalid client session ID")
            return

        # Ensure that client acknowledged our l2 and l3 replies
        if not netjoin_h3.replies_acked():
            logging.error("Client declined l2/l3/gateway-creds response")
            return
        logging.info("Valid handshake three: We can join this network now")

        # Configure Click info
        if len(self.client_hid) < 20:
            logging.error("Client HID not known while handling H3")
            return
        client_hid = str("HID:{}".format(self.client_hid))
        logging.info("Routing table now points to: {}".format(client_hid))
        # Configure routing table
        # interface number is the port for the route
        # client_hid is the entry in HID routing table and also next hop
        # flags == 1 : I'm a host
        flags = 0x0001
        with ClickControl() as click:
            ret = click.setHIDRoute(self.hostname, client_hid, interface, flags)
            if ret == False:
                logging.error("Failed setting route for {}".format(client_hid))
            else:
                logging.info("Route set up for {}".format(client_hid))
            # Inform local xrouted about this HID registration
            self.xrouted.send_host_join(client_hid, interface, flags)

        # Tell client that gateway side configuration is now complete
        logging.info("Sending handshake four")
        client_session = netjoin_h3.get_client_session_id()
        netjoin_h4 = NetjoinHandshakeFour(self, deny=False,
                client_session=client_session)

        # Send handshake four
        self.send_netjoin_message(netjoin_h4, interface, theirmac)

        # We don't need to retransmit handshake 4
        self.disable_retransmission()

        self.state = self.HS_DONE

    def handle_handshake_four(self, message_tuple):
        message, interface, mymac, theirmac = message_tuple

        self.state = self.HS_4_RCVD

        # Disable retransmission of handshake 3
        self.disable_retransmission()

        # Now handle the handshake 4 received over the wire
        logging.info("Got a handshake four message")
        netjoin_h4 = NetjoinHandshakeFour(self)
        netjoin_h4.from_wire_handshake_four(message.handshake_four)
        netjoin_h4.print_handshake_four()
        netjoin_h4.print_cyphertext()

        # Verify the client_session_id matches the encrypted one
        if not netjoin_h4.valid_client_session_id():
            logging.error("Invalid client session ID")
            return False

        # Ensure that client acknowledged our l2 and l3 replies
        if not netjoin_h4.replies_acked():
            logging.error("Gateway had trouble setting up routes to us")
            return False
        logging.info("Valid handshake four: We are on this network now")
        total_joining_time = time.time() - self.start_time
        logging.info("TOTAL joining time: {}ms".format(total_joining_time*1000))
        self.state = self.HS_DONE

        net_id = self.beacon.find_xip_netid()
        assert(net_id is not None)
        ad = "AD:{}".format(self.conf.raw_ad_to_hex(net_id))

        # On routers; Notify xrouted and start announcing this network
        if self.is_router:

            # Routers must have received controller_dag in HS2
            if self.controller_dag == None:
                logging.error("Controller DAG not received in HS2")
                return False

            old_controller_dag = self.conf.get_controller_dag()
            controller_dag_changed = False
            if old_controller_dag != self.controller_dag:
                logging.info("Old controller:{}".format(old_controller_dag))
                logging.info("New controller:{}".format(self.controller_dag))
                controller_dag_changed = True

            # Write the controller_dag config file to etc
            self.conf.write_controller_dag(self.controller_dag)

            # TODO: Need to get l2_type to announce from a config file
            # TODO: Need to get beacon_interval from a config file
            l2_type = LayerTwoIdentifier.ETHERNET
            beacon_interval = 0.5

            # Inform local xrouted that NetJoin is complete
            self.xrouted.send_config(ad, self.controller_dag)

            # Ask the NetjoinReceiver to announce the newly joined network
            if (self.receiver.announcer == None or
                    controller_dag_changed):
                logging.info("No announcer or controller dag changed")
                logging.info("So announce this network")
                self.receiver.announce(l2_type, net_id, beacon_interval)
        else:
            self.xrouted.send_host_config(ad, self.router_hid,
                    self.router_sid, interface)

        return True

    # Note: we forget the last message on disabling retransmission
    # If in future we need enable_retransmission() then use a separate flag
    def disable_retransmission(self):
        self.last_message_tuple = None

    # retransmit message stored in self.last_message_tuple, if any
    # Note: nonce will change on every retransmission
    def retransmit_last_message(self):
        if self.last_message_tuple is None:
            # Silently return if retransmission is disabled
            # TODO: End session some time after sending h4.
            #       After sending h4 on gateway, retransmission is disabled
            #       but the session is still in HS_3_WAIT.
            #       If client doesn't receive h4 it will resend h3
            #       so we should wait around for some time for that h3
            #       by counting down iterations for h4 retransmission.
            #       and finally close the session on reaching 0
            return
        message, interface, theirmac = self.last_message_tuple
        return self.send_netjoin_message(message, interface, theirmac,
                retransmitting=True)

    # Main thread handles all messages based on state of joining session
    def run(self):
        logging.debug("Started session ID: {}".format(self.session_ID))

        while not self.shutdown_event.is_set():

            # Keep waiting for a message indefinitely
            try:
                message_tuple = self.q.get(block=True, timeout=self.q.timeout)
                message = message_tuple[0]
            except (Queue.Empty):
                if self.state != self.START:
                    if self.retransmit_last_message() == False:
                        logging.error("Timed out trying to send a message")
                        break
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

            if self.state == self.BEACON_RCVD:
                if message_type == "net_descriptor":
                    logging.info("Beacon rcvd while handling another ignored")
                else:
                    logging.error("Invalid message: {}".format(message_type))
                    logging.error("Expected net_descriptor")
                continue

            if self.state == self.HS_1_RCVD:
                if message_type == "handshake_one":
                    logging.info("HS1 rcvd while handling another. Ignored")
                else:
                    logging.error("Invalid message:{}".format(message_type))
                    logging.error("Expected handshake_one retransmit only")
                continue

            if self.state == self.HS_2_WAIT:
                if message_type == "handshake_two":
                    self.handle_handshake_two(message_tuple)
                else:
                    logging.error("Invalid message: {}".format(message_type))
                    logging.error("Expected handshake two.")
                continue

            if self.state == self.HS_2_RCVD:
                if message_type == "handshake_two":
                    logging.info("HS2 rcvd while handling another. Ignored")
                else:
                    logging.error("Invalid message: {}".format(message_type))
                    logging.error("Expected handshake two.")
                continue

            if self.state == self.HS_3_WAIT:
                if message_type == "handshake_three":
                    self.handle_handshake_three(message_tuple)
                elif message_type == "handshake_one":
                    self.handle_handshake_one(message_tuple)
                else:
                    logging.error("Invalid message: {}".format(message_type))
                    logging.error("Expected handshake three.")
                continue

            if self.state == self.HS_3_RCVD:
                if message_type == "handshake_three":
                    logging.info("HS3 rcvd while handling another. Ignored")
                else:
                    logging.error("Invalid message: {}".format(message_type))
                    logging.error("Expected handshake three.")
                continue

            if self.state == self.HS_4_WAIT:
                if message_type == "handshake_four":
                    if self.handle_handshake_four(message_tuple) == True:
                        break
                else:
                    logging.error("Invalid message: {}".format(message_type))
                    logging.error("Expected handshake four.")
                continue

            if self.state == self.HS_4_RCVD:
                if message_type == "handshake_four":
                    logging.info("HS4 rcvd while handling another. Ignored")
                else:
                    logging.error("Invalid message: {}".format(message_type))
                    logging.error("Expected handshake four.")
                continue

            if self.state == self.HS_DONE:
                if message_type == "handshake_three":
                    logging.info("Got HS3 after sending HS4, send a new HS4")
                    self.handle_handshake_three(message_tuple)
                else:
                    logging.error("Invalid message: {}".format(message_type))
                    logging.error("Expected handshake three.")
                continue

        if self.receiver is not None:
            self.receiver.session_done(self.get_ID())
        else:
            logging.error("Receiver not found. OK during unit test.")

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

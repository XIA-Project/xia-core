#!/usr/bin/python
#

from google.protobuf import text_format as protobuf_text_format
import logging
import ndap_pb2
import hashlib
import time
import uuid
import nacl.encoding
import nacl.signing
import nacl.utils
from netjoin_l2_handler import NetjoinL2Handler
from netjoin_authsession import NetjoinAuthsession

# This is a wrapper for the NetDescriptor protobuf defined in ndap.proto
class NetjoinBeacon(object):
    def __init__(self, xip_netid=None):
        self.net_descriptor = ndap_pb2.NetDescriptor()
        self.guid = None
        self.xip_netid = xip_netid
        self.xip_hid = None
        self.raw_verify_key = None

    def beacon_str(self):
        return protobuf_text_format.MessageToString(self.net_descriptor)

    def print_beacon(self):
        logging.debug(self.beacon_str())

    def get_ID(self):
        # Copy net_descriptor without nonce and hash it
        fixed_descriptor = ndap_pb2.NetDescriptor()
        fixed_descriptor.CopyFrom(self.net_descriptor)
        fixed_descriptor.ac_shared.ja.ClearField("gateway_nonce")
        # TODO: store hash if this method is called several times
        return hashlib.sha256(fixed_descriptor.SerializeToString()).hexdigest()

    def find_xip_netid(self):
        netid = None

        # Walk the nodes to the end to find XIP network
        # TODO: Create graph and walk like the policy module does
        for node in self.net_descriptor.auth_cap.nodes:
            if node.HasField('xip'):
                netid = node.xip.NetworkId
                break

        return netid

    def _update_object_from_net_descriptor(self):
        # NOTE: we don't have xip_netid received beacon
        self.guid = self.net_descriptor.GUID
        self.raw_verify_key = self.net_descriptor.ac_shared.ja.gateway_ephemeral_pubkey.the_key
        self.xip_netid = self.find_xip_netid()

        assert(self.xip_netid != None)

    def from_net_descriptor(self, net_descriptor):
        self.net_descriptor.CopyFrom(net_descriptor)
        self._update_object_from_net_descriptor()

    # serialized_beacon is actually a serialized ndap_pb2.NetDescriptor
    def from_serialized_net_descriptor(self, serialized_descriptor):
        self.net_descriptor.ParseFromString(serialized_descriptor)
        self._update_object_from_net_descriptor()

    # For now we just build a beacon with a dummy AuthCapStruct
    def initialize(self, raw_verify_key, l2_type, guid=None, xip_hid=None):
        self.guid = guid
        self.xip_hid = xip_hid
        self.raw_verify_key = raw_verify_key

        if self.guid == None:
            self.guid = uuid.uuid4().bytes
            logging.warning("GUID not provided. Assigning a temporary one")

        if self.xip_netid == None:
            logging.error("XIP NID not given.")
            raise RuntimeError("XIP network ID not known.")

        # If l2 type is not known throw an exception and end
        if l2_type not in NetjoinL2Handler.l2_type_info:
            logging.error("Invalid layer two specified")
            raise KeyError(l2_type)

        # Hard-code the cipher suite we are using
        pubkey = self.net_descriptor.ac_shared.ja.gateway_ephemeral_pubkey
        pubkey.cipher_suite = ndap_pb2.AuthCapSharedInfo.JoinAuthExtraInfo.PublicKey.NACL_curve25519xsalsa20poly1305

        # Our network descriptor message
        self.net_descriptor.GUID = self.guid
        self.net_descriptor.l2_id.l2_type = l2_type

        # Build AuthCapStruct showing network authorization and capabilities
        auth_cap = self.net_descriptor.auth_cap

        # 4 nodes including the fictitious root node0
        node1 = auth_cap.nodes.add()
        node2 = auth_cap.nodes.add()
        node3 = auth_cap.nodes.add()

        # Authorization provider 1234 is supported
        node1.ja.version = 0
        node1.ja.aaa_provider = 1234
        node1.ja.context = 0

        # Authorization provider 5678 is supported
        node2.ja.version = 0
        node2.ja.aaa_provider = 5678
        node2.ja.context = 0

        # The network provides XIP with NID xip_netid
        node3.xip.is_global = True
        node3.xip.NetworkId = self.xip_netid
        if self.xip_hid != None:
            node3.xip.HostId = self.xip_hid

        # 4 edges total
        edge1 = auth_cap.edges.add()
        edge2 = auth_cap.edges.add()
        edge3 = auth_cap.edges.add()
        edge4 = auth_cap.edges.add()

        # Provider in node1 can authorize connection to this network
        edge1.from_node = 0
        edge1.to_node = 1

        # Provider in node2 can authorize connection to this network
        edge2.from_node = 0
        edge2.to_node = 2

        # After authorization, XIP network defined in node3 can be joined
        edge3.from_node = 1
        edge3.to_node = 3

        edge4.from_node = 2
        edge4.to_node = 3

    # Create a new random nonce for inclusion in next outgoing beacon
    def update_nonce(self):
        self.net_descriptor.ac_shared.ja.gateway_nonce = nacl.utils.random(2)

    # Copy self.verify_key into self.net_descriptor
    def update_verify_key(self):
        pubkey = self.net_descriptor.ac_shared.ja.gateway_ephemeral_pubkey
        pubkey.the_key = self.raw_verify_key

    # Update the nonce and ephemeral_pubkey in net_descriptor
    def update_and_get_serialized_descriptor(self):
        self.update_nonce()
        self.update_verify_key()
        return self.net_descriptor.SerializeToString()

if __name__ == "__main__":
    beacon = NetjoinBeacon()
    auth = NetjoinAuthsession()
    beacon.initialize(auth.get_raw_verify_key())
    serialized_descriptor = beacon.update_and_get_serialized_descriptor()
    descriptor_size = len(serialized_descriptor)
    logging.debug("Serialized descriptor size: {}".format(descriptor_size))

    # Deserialize and print the contents of beacon again
    new_beacon = NetjoinBeacon()
    new_beacon.from_serialized_net_descriptor(serialized_descriptor)
    new_beacon.print_beacon()
    print "PASSED: NetjoinBeacon"

    # The beacon payload should now be sent to XIANetJoin
    # XIANetJoin will need a header to identify this as a beacon


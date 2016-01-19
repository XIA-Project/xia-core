#!/usr/bin/python
#

from google.protobuf import text_format as protobuf_text_format
import logging
import ndap_pb2
import uuid

def print_descriptor(net_descriptor):
    print protobuf_text_format.MessageToString(net_descriptor)

# For now we just build a beacon with a dummy AuthCapStruct
def build_beacon(guid=None, xip_netid=None):
    if guid == None:
        guid = uuid.uuid4().bytes
        logging.warning("GUID not provided. Assigning a temporary one")
    if xip_netid == None:
        xip_netid = uuid.uuid4().bytes
        logging.warning("XIP NID not giver. Assigning a temporary one")

    # Our network descriptor message
    net_descriptor = ndap_pb2.NetDescriptor()
    net_descriptor.GUID = guid
    net_descriptor.l2_id.l2_type = ndap_pb2.LayerTwoIdentifier.ETHERNET

    # Build AuthCapStruct showing network authorization and capabilities
    auth_cap = net_descriptor.auth_cap

    # 4 nodes including the fictitious root node0
    node1 = auth_cap.nodes.add()
    node2 = auth_cap.nodes.add()
    node3 = auth_cap.nodes.add()

    # Authorization provider 1234 is supported
    node1.id = 1
    node1.ja.version = 0
    node1.ja.aaa_provider = 1234
    node1.ja.context = 0

    # Authorization provider 5678 is supported
    node2.id = 2
    node2.ja.version = 0
    node2.ja.aaa_provider = 5678
    node2.ja.context = 0

    # The network provides XIP with NID xip_netid
    node3.id = 3
    node3.xip.is_global = True
    node3.xip.NetworkId = xip_netid

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

    return net_descriptor.SerializeToString()

if __name__ == "__main__":
    serialized_net_descriptor = build_beacon()
    descriptor_size = len(serialized_net_descriptor)
    logging.debug("Serialized descriptor size: {}".format(descriptor_size))
    #with open("net_descriptor.beacon", "wb") as fd:
    #    fd.write(serialized_net_descriptor)

    # Deserialize and print the contents of beacon again
    new_descriptor = ndap_pb2.NetDescriptor()
    new_descriptor.ParseFromString(serialized_net_descriptor)
    print_descriptor(new_descriptor)

    # The beacon payload should now be sent to XIANetJoin
    # XIANetJoin will need a header to identify this as a beacon


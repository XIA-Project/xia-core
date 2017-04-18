import logging
from netjoin_l2_handler import NetjoinL2Handler
from ndap_pb2 import LayerTwoIdentifier
from jacp_pb2 import LayerTwoRequest, LayerTwoReply

# LayerTwoRequest.EthernetL2Request - for inclusion in HandshakeOne
# LayerTwoReply.EthernetL2Config - for inclusion in HandshakeTwo
class NetjoinEthernetHandler(NetjoinL2Handler):
    def __init__(self):
        logging.info("Initialized ethernet l2 handler")
        NetjoinL2Handler.__init__(self, LayerTwoIdentifier.ETHERNET)

    # Build a LayerTwoRequest.EthernetL2Request
    # to be included in HandshakeOne
    def build_request(self, mac_addr):
        logging.info("building an ethernet request")
        l2_request = LayerTwoRequest()
        l2_request.ethernet.client_mac_address = mac_addr
        return l2_request

    # validate a request to see if we can accept it
    # For now, we just check it is an ethernet request with a sane mac addr
    def _valid_request(self, l2_request):

        if l2_request.WhichOneof('l2_req') != 'ethernet':
            logging.error("Non ethernet request handed to Ethernet handler")
            return False

        if len(l2_request.ethernet.client_mac_address) != 6:
            logging.error("MAC addr not 6 bytes long")
            return False

        # All checks passed, this is an acceptable request
        return True

    # Takes LayerTwoRequest containing EthernetL2Request from HandshakeOne
    # Returns LayerTwoReply with EthernetL2Config for inclusion in HandshakeTwo
    def handle_request(self, l2_request):
        logging.info("handling an ethernet request")

        # Container holding the l2 reply
        l2_reply = LayerTwoReply()

        # If invalid request, return LayerTwoDeny
        if not self._valid_request(l2_request):
            logging.error("Invalid Ethernet request. Generating NACK")
            l2_reply.deny.SetInParent()
            return l2_reply

        # Generate an Ethernet ACK in response
        l2_reply.grant.ethernet.SetInParent()
        return l2_reply

if __name__ == "__main__":
    handler = NetjoinEthernetHandler()
    print "Ethernet msg retransmit rate (sec): {}".format(handler.rate())
    print "Ethernet msg retransmit iterations: {}".format(handler.iterations())
    print "PASSED: NetjoinEthernetHandler test"

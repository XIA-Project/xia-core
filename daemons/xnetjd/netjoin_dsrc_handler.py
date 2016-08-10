import logging
from netjoin_l2_handler import NetjoinL2Handler
from ndap_pb2 import LayerTwoIdentifier
from jacp_pb2 import LayerTwoRequest, LayerTwoReply

# LayerTwoRequest.DSRCL2Request - for inclusion in HandshakeOne
# LayerTwoReply.DSRCL2Config - for inclusion in HandshakeTwo
class NetjoinDSRCHandler(NetjoinL2Handler):
    def __init__(self):
        logging.info("Initialized DSRC l2 handler")
        NetjoinL2Handler.__init__(self, LayerTwoIdentifier.DSRC)

    # Build a LayerTwoRequest.DSRCL2Request
    # to be included in HandshakeOne
    def build_request(self, mac_addr):
        logging.info("building an dsrc request")
        l2_request = LayerTwoRequest()
        l2_request.dsrc.client_mac_address = mac_addr
        return l2_request

    # validate a request to see if we can accept it
    # For now, we just check it is an dsrc request with a sane mac addr
    def _valid_request(self, l2_request):

        if l2_request.WhichOneof('l2_req') != 'dsrc':
            logging.error("Non dsrc request handed to DSRC handler")
            return False

        if len(l2_request.dsrc.client_mac_address) != 6:
            logging.error("MAC addr not 6 bytes long")
            return False

        # All checks passed, this is an acceptable request
        return True

    # Takes LayerTwoRequest containing DSRCL2Request from HandshakeOne
    # Returns LayerTwoReply with DSRCL2Config for inclusion in HandshakeTwo
    def handle_request(self, l2_request):
        logging.info("handling a dsrc request")

        # Container holding the l2 reply
        l2_reply = LayerTwoReply()

        # If invalid request, return LayerTwoDeny
        if not self._valid_request(l2_request):
            logging.error("Invalid DSRC request. Generating NACK")
            l2_reply.deny.SetInParent()
            return l2_reply

        # Generate an DSRC ACK in response
        l2_reply.grant.dsrc.SetInParent()
        return l2_reply

if __name__ == "__main__":
    handler = NetjoinDSRCHandler()
    print "DSRC msg retransmit rate (sec): {}".format(handler.rate())
    print "DSRC msg retransmit iterations: {}".format(handler.iterations())
    print "PASSED: NetjoinDSRCHandler test"

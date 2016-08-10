#!/usr/bin/python
#

from google.protobuf import text_format as protobuf_text_format
import logging
import collections
from ndap_pb2 import LayerTwoIdentifier

# Build a HandshakeThree protobuf in response to a NetDescriptor beacon
class NetjoinL2Handler(object):
    L2Info = collections.namedtuple('L2Info', ['rate', 'iterations'])

    # Table setting retransmission rate and duration for all l2 types
    l2_type_info = {
            LayerTwoIdentifier.ETHERNET:L2Info(rate=0.1, iterations=50),
            LayerTwoIdentifier.WIFI    :L2Info(rate=0.1, iterations=5),
            LayerTwoIdentifier.DSRC    :L2Info(rate=0.2, iterations=50),
            }

    # Table to convert string type to LayerTwoIdentifier.l2_type
    l2_str_to_type = {
            'ethernet': LayerTwoIdentifier.ETHERNET,
            'wifi'    : LayerTwoIdentifier.WIFI,
            'dsrc'    : LayerTwoIdentifier.DSRC,
            }

    def __init__(self, l2_type):
        if l2_type not in self.l2_type_info:
            logging.error("Unknown l2_type {}".format(l2_type))
            raise ValueError("Unknown l2_type {}".format(l2_type))

        self.l2_type = l2_type

    def rate(self):
        return self.l2_type_info[self.l2_type].rate

    def iterations(self):
        return self.l2_type_info[self.l2_type].iterations

    def build_request(self, mac_addr):
        msg = "build_request() must be implemented in l2 handler subclass"
        raise NotImplementedError(msg)

    def handle_request(self, l2_request):
        msg = "handle_request() must be implemented in l2 handler subclass"
        raise NotImplementedError(msg)

if __name__ == "__main__":
    l2_handler = NetjoinL2Handler(LayerTwoIdentifier.ETHERNET)
    print "dummy l2 handler rate (sec): {}".format(l2_handler.rate())
    print "dummy l2 handler iterations: {}".format(l2_handler.iterations())
    print "PASSED: layer 2 handler test"

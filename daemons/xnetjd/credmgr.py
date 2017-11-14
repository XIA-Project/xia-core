#!/usr/bin/env python2.7
#

import os
import sys
import jacp_pb2
import argparse
from netjoin_xiaconf import NetjoinXIAConf
from google.protobuf import text_format as protobuf_text_format

# Include daemons/xnetjd in path no credmgr modules can be loaded
srcdir = os.getcwd()[:os.getcwd().rindex('xia-core')+len('xia-core')]
sys.path.append(os.path.join(srcdir, 'daemons/xnetjd'))

class CredMgr(object):

    def __init__(self):
        # Ensure we are running on a controller
        self.is_controller = False
        self.conf = NetjoinXIAConf()
        self.ad = self.conf.get_ad()
        if self.ad is not None:
            self.is_controller = True

    def issue_router_cred(self):
        # We can issue credentials only when running on controller
        if not self.is_controller:
            raise RuntimeError("Credentials can only be issued on controller")

        # Router credential file name. Will include RHID in future.
        rcredfilepath = os.path.join(srcdir, 'etc/RHID.cred')

        # Build an AD-only credential for now
        router_cred = jacp_pb2.RouterCredentials()
        router_cred.adonly.ad = self.ad
        with open(rcredfilepath, 'w+') as rcredfile:
            rcredfile.write(router_cred.SerializeToString())

    def read_router_cred(self, rcredfilepath):
        router_cred = self.get_router_cred(rcredfilepath)
        if router_cred is not None:
            print protobuf_text_format.MessageToString(router_cred)

    def get_router_cred(self, rcredfilepath):
        router_cred = None
        with open(rcredfilepath) as rcredfile:
            serialized_cred = rcredfile.read()
            router_cred = jacp_pb2.RouterCredentials()
            router_cred.ParseFromString(serialized_cred)
        return router_cred

# Argument parser
def parse_args():
    parser = argparse.ArgumentParser(description="XIA Credential Manager")

    # Issue a router credential. AD only for now.
    parser.add_argument("-r", "--issue_router_cred",
            help="issue router credential", action="store_true")
    # Read in a router credential.
    parser.add_argument("--read_router_cred",
            help="read router credentials from RHID.cred file", type=str)

    return parser.parse_args()

# XIA Credential Manager
def main():
    args = parse_args()
    mgr = CredMgr()

    if args.issue_router_cred:
        mgr.issue_router_cred()

    if args.read_router_cred:
        mgr.read_router_cred(args.read_router_cred)


# Start the credmgr
if __name__ == "__main__":
    main()

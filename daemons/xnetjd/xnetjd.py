#!/usr/bin/env python2.7
#

import os
import sys
import logging
import argparse
import threading
import logging.handlers

# Include daemons/xnetjd in path so network joining py modules can be loaded
srcdir = os.getcwd()[:os.getcwd().rindex('xia-core')+len('xia-core')]
sys.path.append(os.path.join(srcdir, "daemons/xnetjd"))

# Put XIA API in path
from netjoin_xiaconf import NetjoinXIAConf
conf = NetjoinXIAConf()
sys.path.append(conf.get_swig_path())
sys.path.append(conf.get_clickcontrol_path())

from netjoin_policy import NetjoinPolicy
from netjoin_receiver import NetjoinReceiver
from netjoin_announcer import NetjoinAnnouncer
from netjoin_l2_handler import NetjoinL2Handler
from netjoin_authsession import NetjoinAuthsession

# Setup logging for this application
loglevel = {
        0:logging.CRITICAL,
        1:logging.INFO, #logging.ERROR,
        2:logging.INFO, #logging.WARNING,
        3:logging.INFO,
        4:logging.INFO,
        5:logging.INFO,
        6:logging.INFO,
        7:logging.DEBUG
        }
logging.basicConfig(level=logging.INFO, format='%(asctime)s: %(module)s %(levelname)s: %(message)s')

# Argument parser
def parse_args():
    parser = argparse.ArgumentParser(description="XIA Network Joining Manager")
    parser.add_argument("-c", "--client",
            help="process network discovery beacons", action="store_true")
    parser.add_argument("-a", "--accesspoint",
            help="announce network on all interfaces", action="store_true")
    parser.add_argument("-i", "--beacon_interval",
            help="beacon interval in seconds(float)", type=float, default=0.5)
    parser.add_argument("--hostname",
            help="click hostname", type=str)
    parser.add_argument("-l", "--loglevel",
            help="logging level [0-7]", type=int, default=6)
    parser.add_argument("-v", "--verbose",
            help="verbosity on stderr", action="store_true")
    parser.add_argument("-q", "--quiet",
            help="reduced verbosity", action="store_true")
    parser.add_argument("--layer2",
            help="layer 2 type", type=str)
    return parser.parse_args()

# Parse arguments and launch necessary threads
def main():

    # Parse user provided arguments
    args = parse_args()

    # Set up logging
    logger = logging.getLogger()
    logger.setLevel(loglevel[args.loglevel])
    print "logger level: {} in response to loglevel {}".format(loglevel[args.loglevel], args.loglevel)
    sysloghandler = logging.handlers.SysLogHandler(address="/dev/log")
    logger.addHandler(sysloghandler)
    if args.verbose:
        stdouthandler = logging.StreamHandler(stream=sys.stderr)
        logger.addHandler(stdouthandler)

    # All threads must listen for this event and exit gracefully when set
    shutdown_event = threading.Event()

    # List of all threads
    threads = []

    # Initialize the policy module
    # For now we have one policy for all beacons.
    # TODO: Pass list of auth providers
    policy = NetjoinPolicy()

    announcer = None
    if args.accesspoint:
        logging.debug("Announcing network and listening for join requests")
        # Determine the Layer2 type for retransmission rate and attempts
        l2_type = NetjoinL2Handler.l2_str_to_type[args.layer2]
        # Start an auth session for announcer
        announcer = NetjoinAnnouncer(l2_type, args.beacon_interval,
                shutdown_event)
        announcer.announce()

    if args.client:
        logging.debug("Listening for network announcements")

    # Start a new thread to listen for messages
    receiver = NetjoinReceiver(args.hostname, policy, announcer, shutdown_event)
    receiver.daemon = True
    receiver.start()
    threads.append(receiver)

    # We block here until the user interrupts or kills the program
    try:
        for thread in threads:
            while thread.is_alive():
                thread.join(timeout=1.0)
    except (KeyboardInterrupt, SystemExit):
        shutdown_event.set()

if __name__ == "__main__":
    main()

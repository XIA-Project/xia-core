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

from clickcontrol import ClickControl
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
    parser.add_argument("-a", "--controller",
            help="announce network on all interfaces", action="store_true")
    parser.add_argument("-r", "--router",
            help="join network then announce it", action="store_true")
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
    # print "logger level: {} in response to loglevel {}".format(loglevel[args.loglevel], args.loglevel)
    sysloghandler = logging.handlers.SysLogHandler(address="/dev/log")
    logger.addHandler(sysloghandler)
    if args.verbose:
        stdouthandler = logging.StreamHandler(stream=sys.stderr)
        logger.addHandler(stdouthandler)
    else:
        # make sure we aren't logging to stdout!
        logger.removeHandler(logger.handlers[0])

    # All threads must listen for this event and exit gracefully when set
    shutdown_event = threading.Event()

    # List of all threads
    threads = []

    # Initialize the policy module
    # For now we have one policy for all beacons.
    # TODO: Pass list of auth providers
    policy = NetjoinPolicy(is_controller=args.controller, is_router=args.router)

    announcer = None
    if args.controller:
        logging.debug("Announcing network and listening for join requests")

        # Read in various DAGs from config files and assign them into Click
        ns_dag = conf.get_ns_dag()
        router_dag = conf.get_router_dag()
        rv_dag = conf.get_rv_dag()
        rv_control_dag = conf.get_rv_control_dag()
        with ClickControl() as click:
            if click.setNSDAG(ns_dag) == False:
                logging.error("Failed updating NS DAG in XIA Stack")
            logging.info("Nameserver DAG is now known to Click stack")
            # TODO: Set router/rv dags into click somewhere
            if rv_dag:
                if click.assignRVDAG(args.hostname, rv_dag) == False:
                    logging.error("Failed updating RV DAG")
                logging.info("RV DAG now known to Click stack")
            if rv_control_dag:
                if click.assignRVControlDAG(args.hostname,
                        rv_control_dag) == False:
                    logging.error("Failed updating RV Control DAG")
                logging.info("RV Control DAG now known to Click stack")


        # Determine the Layer2 type for retransmission rate and attempts
        l2_type = NetjoinL2Handler.l2_str_to_type[args.layer2]

        # Read in the network ID (AD) from etc/address.conf
        net_id = conf.get_raw_ad()

        # Start an auth session for announcer
        announcer = NetjoinAnnouncer(l2_type, net_id, args.beacon_interval,
                shutdown_event)
        announcer.announce()

    if args.client:
        logging.debug("Listening for network announcements")

    if args.router:
        logging.debug("Waiting to hear from other routers or controller")

    # Start a new thread to listen for messages
    receiver = NetjoinReceiver(args.hostname, policy, announcer,
            shutdown_event, is_router=args.router)
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

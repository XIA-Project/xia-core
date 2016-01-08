#!/usr/bin/python
#

import time
import socket
import argparse
import threading
import ndap_pb2
import ndap_beacon

# Announce the presence of this network
def network_announcer(beacon_interval, serialized_beacon, sockfd, xianetjoin):
    # Send beacon to XIANetJoin
    print time.time(), "Sent beacon"
    sockfd.sendto(serialized_beacon, xianetjoin)

    # Call ourselves from a new thread after some time
    args = (beacon_interval, serialized_beacon, sockfd, xianetjoin)
    threading.Timer(beacon_interval, network_announcer, args).start()

def announce_network(beacon_interval):
    xianetjoin = ("127.0.0.1", 9882)

    # A socket for sending messages to XIANetJoin
    sockfd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    # A serialized beacon advertizing this network
    serialized_beacon = ndap_beacon.build_beacon()

    # Announce the beacon to the world
    network_announcer(beacon_interval, serialized_beacon, sockfd, xianetjoin)


# Listen for incoming beacons
def beacon_handler():
    xnetjd = ("127.0.0.1", 9228)
    sockfd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sockfd.bind(xnetjd)
    while True:
        data, addr = sockfd.recvfrom(1034)
        print "Received beacon from", addr
        # Convert beacon to protobuf and print its contents
        dataarray = bytearray(data)
        interface = int(dataarray[0:2])
        sendermac = dataarray[2:8]
        beaconstr = str(dataarray[9:])
        # First two bytes contain interface number
        print "Received on interface:" , interface
        # Six bytes contain source mac address
        print "Sent by:", sendermac
        # Remaining data is the beacon
        beacon = ndap_pb2.NetDescriptor()
        beacon.ParseFromString(beaconstr)
        print "Beacon contained:"
        ndap_beacon.print_descriptor(beacon)

# Argument parser
def parse_args():
    parser = argparse.ArgumentParser(description="XIA Network Joining Manager")
    parser.add_argument("-c", "--client", help="process network discovery beacons", action="store_true")
    parser.add_argument("-a", "--accesspoint", help="provide access point beacons on all interfaces", action="store_true")
    parser.add_argument("-i", "--beacon_interval", help="beacon interval in seconds(float)", type=float, default=0.5)
    return parser.parse_args()

if __name__ == "__main__":

    # Parse user provided arguments
    args = parse_args()

    if args.accesspoint:
        print "Announcing network"
        announce_network(args.beacon_interval)

    if args.client:
        print "Listening for available networks"
        beacon_handler()
    #new_descriptor = ndap_pb2.NetDescriptor()
    #new_descriptor.ParseFromString(serialized_beacon)
    #ndap_beacon.print_descriptor(new_descriptor)

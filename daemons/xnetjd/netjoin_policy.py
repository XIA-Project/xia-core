#! /usr/bin/env python
#

import logging
import hashlib
import ndap_pb2
import ndap_beacon

class NetjoinPolicy:

    JOINED, JOINING, UNJOINABLE = range(3)

    def __init__(self):
        logging.debug("Policy module initialized")
        self.known_beacons = {}
        # Load list of available auth providers from config file

    def print_known_beacons(self):
        print self.known_beacons.keys()

    def get_beacon_id(self, beacon):
        return hashlib.sha256(beacon.SerializeToString()).hexdigest()

    def is_known_beacon(self, beacon):
        beacon_ID = self.get_beacon_id(beacon)
        if beacon_ID in self.known_beacons:
            return True
        return False

    def keep_known_beacon_id(self, beacon_ID, state):
        if beacon_ID in self.known_beacons:
            logging.debug("Overwriting beacon ID: %s" % beacon_ID)
        self.known_beacons[beacon_ID] = state

    def remove_known_beacon_id(self, beacon_ID):
        if beacon_ID in self.known_beacons:
            del self.known_beacons[beacon_ID]
        else:
            logging.error("Removing non-existing beacon: %s" % beacon_ID)

    def is_joinable(beacon):
        # Sanity check the beacon
        # Walk the graph to find a path matching available auth providers
        pass

    def get_serialized_beacon_id(serialized_beacon):
        return hashlib.sha256(serialized_beacon).hexdigest()

    def is_known_serialized_beacon(self, serialized_beacon):
        beacon_ID = get_serialized_beacon_id(serialized_beacon)
        if beacon_ID in self.known_beacons:
            return True
        return False

    # Main entry point for the policy module
    def process_serialized_beacon(serialized_beacon):
        # If this beacon has been processed before, drop it
        if is_known_serialized_beacon(serialized_beacon):
            return
        # Convert beacon to an object we can look into
        beacon = ndap_pb2.NetDescriptor()
        beacon.ParseFromString(serialized_beacon)
        if is_joinable(beacon):
            # Initiate Netjoiner action and add to known_beacons as JOINING
            pass
        else:
            # Add to known_beacons as UNJOINABLE
            pass

# Unit test this module when run by itself
if __name__ == "__main__":
    policy = NetjoinPolicy()
    serialized_test_beacon = ndap_beacon.build_beacon()
    test_beacon = ndap_pb2.NetDescriptor()
    test_beacon.ParseFromString(serialized_test_beacon)
    beacon_ID = policy.get_beacon_id(test_beacon)
    policy.keep_known_beacon_id(beacon_ID, NetjoinPolicy.JOINING)
    policy.print_known_beacons()
    policy.remove_known_beacon_id(beacon_ID)
    policy.print_known_beacons()

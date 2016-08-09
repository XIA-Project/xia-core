#! /usr/bin/python
#

import time
import logging
import threading

class NetjoinAffinity(object):
    """Affinity to a given beacon on a given interface.

    Maintain an affinity metric for each combination of (beacon, interface).
    The policy module can then query the current affinity metric for
    inclusion in policy decisions regarding joining a network.

    Attributes:
        MAX_AFFINITY: prevent affinity from rising indefinitely due to
            malicious or corrupt access point code.
        MIN_AFFINITY: prevent affinity from falling indefinitely due to
            loss of connection or extended interruption.
        PREFER_THRESHOLD: difference between two affinities to consider
            switching.
    """
    MAX_AFFINITY = 2
    MIN_AFFINITY = -10
    THRESHOLD = 5

    def __init__(self, decrement_interval=1.0):
        """Start with no known affinity to any beacon or interface."""
        self.stop = False
        self.affinity = {}
        self.interval = decrement_interval

    def print_affinity(self):
        for ((beacon, iface), value) in self.affinity.items():
            logging.debug("{}\t{}\t{}".format(beacon, iface, value))

    def set_interval(self):
        self.interval = interval

    def prefer_over(self, key1, key2):
        """Prefer a (beacon, interface) over another (beacon, interface).

        We prefer key1 over key2 if the affinity is >PREFER_THRESHOLD.

        Args:
            key1: (beacon, interface) affinity of new network consideration
            key2: (beacon, interface) affinity of existing connection

        Returns:
            True if the first (beacon, interface) is preferred
        """

        if key1[1] != key2[1]:
            logging.error("Mismatched interface during preference comparison")
            return False

        if self.affinity[key1] > (self.affinity[key2] + self.THRESHOLD):
            return True

        return False

    def get(self, beacon, interface):
        """Get affinity value for a given beacon on an interface.

        Returns:
            Affinity to a given beacon on the specified interface

        Raises:
            KeyError: (beacon, interface) affinity not found.
        """

        return self.affinity[(beacon, interface)]

    def update(self, beacon, interface):
        """Increment affinity value for (beacon, interface).

        Args:
            beacon: beacon identifier calculated by NetjoinReceiver
            interface: on which the beacon came in
        """

        key = (beacon, interface)
        if key not in self.affinity:
            self.affinity[key] = 0;
            return

        if self.affinity[key] < self.MAX_AFFINITY:
            self.affinity[key] += 1

    def periodic_decrement(self):
        """Decrement all affinity values by 1.

        Called periodically at the same rate as expected
        retransmission of beacons.

        Todo:
            * The timer needs to be per interface based on its L2
        """

        if self.stop is True:
            return

        for (key, value) in self.affinity.items():
            if value > self.MIN_AFFINITY:
                self.affinity[key] -= 1
                logging.debug("---Affinity after decrement---")
                self.print_affinity()

        threading.Timer(self.interval, self.periodic_decrement).start()

    def end(self):
        """End the periodic decrement thread."""
        self.stop = True

if __name__ == "__main__":
    logformat = '%(asctime)s: %(module)s %(levelname)s: %(message)s'
    logging.basicConfig(level=logging.DEBUG, format=logformat)

    affinity = NetjoinAffinity()
    affinity.update('beacon1', 2)
    affinity.update('beacon2', 1)
    logging.debug("---Initial affinity---")
    affinity.print_affinity()
    logging.debug("---Initial affinity end---")

    affinity.update('beacon1', 2)
    affinity.update('beacon1', 2)
    affinity.update('beacon2', 1)
    affinity.update('beacon2', 1)
    affinity.update('beacon2', 1)

    affinity.print_affinity()
    affinity.periodic_decrement()

    time.sleep(9)
    affinity.end()


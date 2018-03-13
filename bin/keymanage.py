#!/usr/bin/python
#
import re
import os
import os.path
import sys
import xiapyutils

try:
    import psutil
except ImportError:
    print 'keymanage.py: Please install python-psutil package'
    sys.exit(-1)

class KeyManager:
    _keydirname = 'key'
    _addrconfname = 'etc/xia.conf'
    _click_procname = 'click/userlevel/click'

    _xid_pattern = re.compile("([a-zA-Z]+:[0-9a-fA-F]{40})")

    def _srcdir(self):
        return xiapyutils.xia_srcdir()

    def get_keydir(self):
        return os.path.join(self._srcdir(), self._keydirname);

    def get_addrconfpath(self):
        return os.path.join(self._srcdir(), self._addrconfname)

    def _valid_keydir(self, keydir):
        if not os.path.exists(keydir):
            return False
        return True

    def _save_keys(self):
        save_keys = []
        addrconfpath = self.get_addrconfpath()

        # Read the address.conf
        if not os.path.exists(addrconfpath):
            return save_keys

        with open(addrconfpath) as addrconf:
            for line in addrconf:
                for xid in self._xid_pattern.findall(line):
                    save_keys.append(xid.split(':')[1])
        return save_keys

    def _remove_keys_except(self, keydir, save_keys, test):
        for filename in os.listdir(keydir):
            filekeyhash = os.path.splitext(filename)[0]

            # Skip keys that were to be saved
            if filekeyhash in save_keys:
                continue

            # Remove others
            filepath = os.path.join(keydir, filename)

            # Remove only if not testing
            if not test:
                print "Removing {}".format(filepath)
                try:
                    os.remove(filepath)
                except OSError as err:
                    print 'Remove failed {}'.format(err)
            else:
                print "Test rm {}".format(filepath)

    def _click_running(self):
        for pid in psutil.pids():
            p = psutil.Process(pid)
            click_procpath = os.path.join(self._srcdir(), self._click_procname)
            if click_procpath in p.cmdline():
                return True
        return False

    # Clean up the key directory
    def clean(self, test=False):

        # Ensure Click is not running
        if self._click_running():
            print "Click must be stopped before cleaning up key files"
            return False

        # Find where keys are stored
        keydir = self.get_keydir()

        # Sanity check the key directory
        if not self._valid_keydir(keydir):
            print "Invalid key dir: {}".format(keydir)
            return False

        # Find the keys we want to keep
        save_keys = self._save_keys()
        print "Saving these keys:"
        print save_keys
        print

        # Remove all keys except ones we want to keep
        self._remove_keys_except(keydir, save_keys, test)

        # Cleanup done
        return True

if __name__ == "__main__":
    msg = 'Testing KeyManager module'
    print '='* len(msg)
    print msg
    print '='* len(msg)
    key_manager = KeyManager()
    if not key_manager.clean(test=True):
        print "Cleanup of old keys FAILED"
        sys.exit(-1)

#!/usr/bin/env python
#

import keymanage

if __name__ == "__main__":
    key_manager = keymanage.KeyManager()
    if not key_manager.clean():
        print "Key cleanup failed or aborted"

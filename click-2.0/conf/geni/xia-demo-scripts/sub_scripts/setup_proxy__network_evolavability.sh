#!/bin/bash

ssh -A mberman@pg56.emulab.net -f  "cd xia-core/proxies; sleep 5; sudo killall -9 proxy.py; ./proxy.py 7500; exit "






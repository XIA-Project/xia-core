#!/bin/bash

ssh -A mberman@pg40.emulab.net -f  "cd xia-core/proxies; sleep 5; sudo killall -9 webserver.py; ./webserver.py; exit "

ssh -A mberman@pg40.emulab.net -f  "cd xia-core/web_demo; sleep 5; sudo killall -9 stock_service.py; ./stock_service.py; exit "

#ssh -A mberman@pg40.emulab.net -f  "cd xia-core/XIASocket/sample; sleep 5; sudo killall -9 vs; ./vs video.ogv; exit "


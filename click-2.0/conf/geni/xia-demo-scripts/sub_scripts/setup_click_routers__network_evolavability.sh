#!/bin/bash

ssh -A mberman@pg56.emulab.net -f  "cd xia-core/click-2.0; sudo killall -9 click; sudo userlevel/click conf/geni/host0-pg56.click ; exit"

ssh -A mberman@pg55.emulab.net -f  "cd xia-core/click-2.0; sudo killall -9 click; sudo userlevel/click conf/geni/router0-pg55.click; exit"

ssh -A mberman@pg42.emulab.net -f  "cd xia-core/click-2.0; sudo killall -9 click; sudo userlevel/click conf/geni/router1-pg42.click; exit"

ssh -A mberman@pg40.emulab.net -f "cd xia-core/click-2.0; sudo killall -9 click; sudo userlevel/click conf/geni/host1-pg40.click; exit"








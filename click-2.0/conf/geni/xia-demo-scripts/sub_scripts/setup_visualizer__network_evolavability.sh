#!/bin/bash

ssh -A mberman@pg55.emulab.net -f  "cd vis-scripts; sudo killall -9 python prep1.py;  python prep1.py; exit "
ssh -A mberman@pg55.emulab.net -f  "cd vis-scripts; sleep 5; sudo killall -9 python updateForwardingRate.py H0-R0 utah; sh ../xia-core/click-2.0/conf/geni/stat/link-state-pg55.sh | python updateForwardingRate.py H0-R0 utah; exit "
ssh -A mberman@pg55.emulab.net -f  "cd vis-scripts; sleep 5; sudo killall -9 python updateState.py R0 utah; echo Fallback | python updateState.py R0 utah; exit "


ssh -A mberman@pg42.emulab.net -f  "cd vis-scripts; sudo killall -9 python prep1.py;  python prep1.py; exit "
ssh -A mberman@pg42.emulab.net -f  "cd vis-scripts; sleep 5; sudo killall -9 python updateForwardingRate.py R0-R1 utah; sh ../xia-core/click-2.0/conf/geni/stat/link-state-pg42.sh | python updateForwardingRate.py R0-R1 utah; exit "
ssh -A mberman@pg42.emulab.net -f  "cd vis-scripts; sleep 5; sudo killall -9 python updateState.py R1 utah; echo Forwarding | python updateState.py R1 utah; exit "


ssh -A mberman@pg40.emulab.net -f  "cd vis-scripts; sudo killall -9 python prep1.py;  python prep1.py; exit "
ssh -A mberman@pg40.emulab.net -f  "cd vis-scripts; sleep 5; sudo killall -9 python updateForwardingRate.py R1-H1 utah; sh ../xia-core/click-2.0/conf/geni/stat/link-state-pg40.sh | python updateForwardingRate.py R1-H1 utah; exit "
ssh -A mberman@pg40.emulab.net -f  "cd vis-scripts; sleep 5; sudo killall -9 python updateState.py H1 utah; echo Server | python updateState.py H1 utah; exit "


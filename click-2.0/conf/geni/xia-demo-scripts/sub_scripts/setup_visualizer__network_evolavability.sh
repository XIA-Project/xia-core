#!/bin/bash

ssh -A mberman@pg56.emulab.net -f  "cd vis-scripts; sudo killall -9 python updateGraphic.py utah;  exit "
ssh -A mberman@pg56.emulab.net -f  "sleep 5; echo 2 | python ~/vis-scripts/updateGraphic.py utah; exit "

ssh -A mberman@pg55.emulab.net -f  "cd vis-scripts; sudo killall -9 python prep1.py;  python prep1.py; exit "
ssh -A mberman@pg55.emulab.net -f  "cd vis-scripts; sudo killall -9 python updateForwardingRate.py H0-R0 utah; exit"
ssh -A mberman@pg55.emulab.net -f  "sleep 5; sh ~/xia-core/click-2.0/conf/geni/stats/xia-link-state.sh | python ~/vis-scripts/updateForwardingRate.py H0-R0 utah; exit "
ssh -A mberman@pg55.emulab.net -f  "cd vis-scripts; sudo killall -9 python updateState.py R0 utah; exit "
ssh -A mberman@pg55.emulab.net -f  "sleep 5; echo Forwarding | python ~/vis-scripts/updateState.py R0 utah; exit "


ssh -A mberman@pg42.emulab.net -f  "cd vis-scripts; sudo killall -9 python prep1.py;  python prep1.py; exit "
ssh -A mberman@pg42.emulab.net -f  "cd vis-scripts; sudo killall -9 python updateForwardingRate.py R0-R1 utah; exit"
ssh -A mberman@pg42.emulab.net -f  "sleep 5; sh ~/xia-core/click-2.0/conf/geni/stats/xia-link-state.sh | python ~/vis-scripts/updateForwardingRate.py R0-R1 utah; exit "
ssh -A mberman@pg42.emulab.net -f  "cd vis-scripts; sudo killall -9 python updateState.py R1 utah; exit "
ssh -A mberman@pg42.emulab.net -f  "sleep 5; echo Forwarding | python ~/vis-scripts/updateState.py R1 utah; exit "

ssh -A mberman@pg40.emulab.net -f  "cd vis-scripts; sudo killall -9 python prep1.py;  python prep1.py; exit "
ssh -A mberman@pg40.emulab.net -f  "cd vis-scripts; sudo killall -9 python updateForwardingRate.py R1-H1 utah; exit"
ssh -A mberman@pg40.emulab.net -f  "sleep 5; sh ~/xia-core/click-2.0/conf/geni/stats/xia-link-state.sh | python ~/vis-scripts/updateForwardingRate.py R1-H1 utah; exit "
ssh -A mberman@pg40.emulab.net -f  "cd vis-scripts; sudo killall -9 python updateState.py H1 utah; exit "
ssh -A mberman@pg40.emulab.net -f  "sleep 5; echo Forwarding | python ~/vis-scripts/updateState.py H1 utah; exit "


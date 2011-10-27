#!/bin/bash

ssh -A mberman@pg56.emulab.net -f  "cd xia-core; git pull; cd click-2.0; sudo make; cd ../XIASocket/API; make; cd ../sample; make; exit "

ssh -A mberman@pg55.emulab.net -f  "cd xia-core; git pull; cd click-2.0; sudo make; cd ../XIASocket/API; make; cd ../sample; make; exit "

ssh -A mberman@pg42.emulab.net -f  "cd xia-core; git pull; cd click-2.0; sudo make; cd ../XIASocket/API; make; cd ../sample; make; exit "

ssh -A mberman@pg40.emulab.net -f  "cd xia-core; git pull; cd click-2.0; sudo make; cd ../XIASocket/API; make; cd ../sample; make; exit "



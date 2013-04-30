#/bin/sh

# adding basic routing infor XIA routers for testing purpose
(sleep 1;
echo write ad1xionrouter/xrc/n/proc/rt_HID.set4 HID1,0,HID1,0 | nc localhost 7777;
echo write ad2xionrouter/xrc/n/proc/rt_HID.set4 HID2,0,HID2,0 | nc localhost 7777;
echo write ad3xionrouter/xrc/n/proc/rt_HID.set4 HID3,0,HID3,0 | nc localhost 7777;
echo "BASIC ROUTING SETTING DONE FOR XION TESTING PURPOSES";
) &
(cd ../../etc/click/templates; ../../../click/userlevel/click -R ../xion_test_topology.click)

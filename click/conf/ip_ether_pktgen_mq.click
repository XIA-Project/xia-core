define($COUNT 10000000);
define($PAYLOAD_SIZE 1300);
define($HEADROOM_SIZE 148);
define($OUTDEVICE eth2);

// aliases for XIDs
XIAXIDInfo(
    HID0 HID:0000000000000000000000000000000000000000,
    HID1 HID:0000000000000000000000000000000000000001,
    AD0 AD:1000000000000000000000000000000000000000,
    AD1 AD:1000000000000000000000000000000000000001,
    RHID0 HID:0000000000000000000000000000000000000002,
    RHID1 HID:0000000000000000000000000000000000000003,
    CID0 CID:2000000000000000000000000000000000000001,
);


gen1 :: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
-> Script(TYPE PACKET, write gen1.active false)       // stop source after exactly 1 packet
-> Unqueue()
-> UDPIPEncap(10.0.1.2, 5002, 10.0.2.2, 5002)
-> EtherEncap(0x0800, 00:1a:92:9b:4a:77 ,00:15:17:51:d3:d4)
-> Clone($COUNT)
-> td1 :: MQToDevice($OUTDEVICE, QUEUE 0, BURST 32);
StaticThreadSched(td1 0);

gen2 :: InfiniteSource(LENGTH $PAYLOAD_SIZE, ACTIVE false, HEADROOM $HEADROOM_SIZE)
-> Script(TYPE PACKET, write gen2.active false)       // stop source after exactly 1 packet
-> Unqueue()
-> UDPIPEncap(10.0.1.2, 5002, 10.0.2.2, 5002)
-> EtherEncap(0x0800, 00:1a:92:9b:4a:77 ,00:15:17:51:d3:d4)
-> Clone($COUNT)
-> td2 :: MQToDevice($OUTDEVICE, QUEUE 1, BURST 32);
StaticThreadSched(td2 1);

MQPollDevice($OUTDEVICE) -> Discard;

Script(write gen1.active true);
Script(write gen2.active true);

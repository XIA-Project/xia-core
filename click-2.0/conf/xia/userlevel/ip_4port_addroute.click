// Generated by make-ip-conf.pl
//xge0 10.0.0.1 xge0 
//xge1 10.0.1.1 xge1 
//xge2 10.0.2.1 xge2 
//xge3 10.0.3.1 xge3 
define ($BURST 32)

// Shared IP input path and routing table
ip :: Strip(14)  
    -> CheckIPHeader2() //INTERFACES 10.0.0.1/255.255.255.0 10.0.1.1/255.255.255.0 10.0.2.1/255.255.255.0 10.0.3.1/255.255.255.0)
    -> rt :: RadixIPLookup(
	10.0.0.0/255.255.255.0 1,
	10.0.1.0/255.255.255.0 2,
	10.0.2.0/255.255.255.0 3,
	10.0.3.0/255.255.255.0 4,
);

// ARP responses are copied to each ARPQuerier and the host.
//arpt :: Tee(5);

c0 :: Classifier(12/0806 20/0001, 12/0806 20/0002, 12/0800, -);

pd_xge0_0:: MQPollDevice(xge0, QUEUE 0, BURST $BURST, PROMISC true) -> c0; 
pd_xge0_1:: MQPollDevice(xge0, QUEUE 1, BURST $BURST, PROMISC true) -> c0; 
pd_xge0_2:: MQPollDevice(xge0, QUEUE 2, BURST $BURST, PROMISC true) -> c0; 
pd_xge0_3:: MQPollDevice(xge0, QUEUE 3, BURST $BURST, PROMISC true) -> c0; 
pd_xge0_4:: MQPollDevice(xge0, QUEUE 4, BURST $BURST, PROMISC true) -> c0; 
pd_xge0_5:: MQPollDevice(xge0, QUEUE 5, BURST $BURST, PROMISC true) -> c0; 
pd_xge0_6:: MQPollDevice(xge0, QUEUE 6, BURST $BURST, PROMISC true) -> c0; 
pd_xge0_7:: MQPollDevice(xge0, QUEUE 7, BURST $BURST, PROMISC true) -> c0; 
pd_xge0_8:: MQPollDevice(xge0, QUEUE 8, BURST $BURST, PROMISC true) -> c0; 
pd_xge0_9:: MQPollDevice(xge0, QUEUE 9, BURST $BURST, PROMISC true) -> c0; 
pd_xge0_10:: MQPollDevice(xge0, QUEUE 10, BURST $BURST, PROMISC true) -> c0; 
pd_xge0_11:: MQPollDevice(xge0, QUEUE 11, BURST $BURST, PROMISC true) -> c0; 

out0 :: IsoCPUQueue(200);
out0 ->tod_xge0_0 :: MQToDevice(xge0, QUEUE 0, BURST $BURST) 
out0 ->tod_xge0_1 :: MQToDevice(xge0, QUEUE 1, BURST $BURST) 
out0 ->tod_xge0_2 :: MQToDevice(xge0, QUEUE 2, BURST $BURST) 
out0 ->tod_xge0_3 :: MQToDevice(xge0, QUEUE 3, BURST $BURST) 
out0 ->tod_xge0_4 :: MQToDevice(xge0, QUEUE 4, BURST $BURST) 
out0 ->tod_xge0_5 :: MQToDevice(xge0, QUEUE 5, BURST $BURST) 
out0 ->tod_xge0_6 :: MQToDevice(xge0, QUEUE 6, BURST $BURST) 
out0 ->tod_xge0_7 :: MQToDevice(xge0, QUEUE 7, BURST $BURST) 
out0 ->tod_xge0_8 :: MQToDevice(xge0, QUEUE 8, BURST $BURST) 
out0 ->tod_xge0_9 :: MQToDevice(xge0, QUEUE 9, BURST $BURST) 
out0 ->tod_xge0_10 :: MQToDevice(xge0, QUEUE 10, BURST $BURST) 
out0 ->tod_xge0_11 :: MQToDevice(xge0, QUEUE 11, BURST $BURST) 

c0[0] -> Discard;
//c0[0] -> ar0 :: ARPResponder(10.0.0.1 xge0) -> out0;
//arpq0 :: ARPQuerier(10.0.0.1, xge0) -> out0;
//c0[1] -> arpt;
//arpt[0] -> [1]arpq0;
c0[1] -> Discard; //ARP response
c0[2] -> Paint(1) -> ip;
c0[3] -> Print("xge0 non-IP") -> Discard;



c1 :: Classifier(12/0806 20/0001, 12/0806 20/0002, 12/0800, -);

pd_xge1_0:: MQPollDevice(xge1, QUEUE 0, BURST $BURST, PROMISC true) -> c1; 
pd_xge1_1:: MQPollDevice(xge1, QUEUE 1, BURST $BURST, PROMISC true) -> c1; 
pd_xge1_2:: MQPollDevice(xge1, QUEUE 2, BURST $BURST, PROMISC true) -> c1; 
pd_xge1_3:: MQPollDevice(xge1, QUEUE 3, BURST $BURST, PROMISC true) -> c1; 
pd_xge1_4:: MQPollDevice(xge1, QUEUE 4, BURST $BURST, PROMISC true) -> c1; 
pd_xge1_5:: MQPollDevice(xge1, QUEUE 5, BURST $BURST, PROMISC true) -> c1; 
pd_xge1_6:: MQPollDevice(xge1, QUEUE 6, BURST $BURST, PROMISC true) -> c1; 
pd_xge1_7:: MQPollDevice(xge1, QUEUE 7, BURST $BURST, PROMISC true) -> c1; 
pd_xge1_8:: MQPollDevice(xge1, QUEUE 8, BURST $BURST, PROMISC true) -> c1; 
pd_xge1_9:: MQPollDevice(xge1, QUEUE 9, BURST $BURST, PROMISC true) -> c1; 
pd_xge1_10:: MQPollDevice(xge1, QUEUE 10, BURST $BURST, PROMISC true) -> c1; 
pd_xge1_11:: MQPollDevice(xge1, QUEUE 11, BURST $BURST, PROMISC true) -> c1; 
out1 :: IsoCPUQueue(200);
out1 ->tod_xge1_0 :: MQToDevice(xge1, QUEUE 0, BURST $BURST) 
out1 ->tod_xge1_1 :: MQToDevice(xge1, QUEUE 1, BURST $BURST) 
out1 ->tod_xge1_2 :: MQToDevice(xge1, QUEUE 2, BURST $BURST) 
out1 ->tod_xge1_3 :: MQToDevice(xge1, QUEUE 3, BURST $BURST) 
out1 ->tod_xge1_4 :: MQToDevice(xge1, QUEUE 4, BURST $BURST) 
out1 ->tod_xge1_5 :: MQToDevice(xge1, QUEUE 5, BURST $BURST) 
out1 ->tod_xge1_6 :: MQToDevice(xge1, QUEUE 6, BURST $BURST) 
out1 ->tod_xge1_7 :: MQToDevice(xge1, QUEUE 7, BURST $BURST) 
out1 ->tod_xge1_8 :: MQToDevice(xge1, QUEUE 8, BURST $BURST) 
out1 ->tod_xge1_9 :: MQToDevice(xge1, QUEUE 9, BURST $BURST) 
out1 ->tod_xge1_10 :: MQToDevice(xge1, QUEUE 10, BURST $BURST) 
out1 ->tod_xge1_11 :: MQToDevice(xge1, QUEUE 11, BURST $BURST) 

c1[0] -> Discard;
//c1[0] -> ar1 :: ARPResponder(10.0.1.1 xge1) -> out1;
//arpq1 :: ARPQuerier(10.0.1.1, xge1) -> out1;
//c1[1] -> arpt;
//arpt[1] -> [1]arpq1;
c1[1] -> Discard; //ARP response
c1[2] -> Paint(2) -> ip;
c1[3] -> Print("xge1 non-IP") -> Discard;



c2 :: Classifier(12/0806 20/0001, 12/0806 20/0002, 12/0800, -);

pd_xge2_0:: MQPollDevice(xge2, QUEUE 0, BURST $BURST, PROMISC true) -> c2; 
pd_xge2_1:: MQPollDevice(xge2, QUEUE 1, BURST $BURST, PROMISC true) -> c2; 
pd_xge2_2:: MQPollDevice(xge2, QUEUE 2, BURST $BURST, PROMISC true) -> c2; 
pd_xge2_3:: MQPollDevice(xge2, QUEUE 3, BURST $BURST, PROMISC true) -> c2; 
pd_xge2_4:: MQPollDevice(xge2, QUEUE 4, BURST $BURST, PROMISC true) -> c2; 
pd_xge2_5:: MQPollDevice(xge2, QUEUE 5, BURST $BURST, PROMISC true) -> c2; 
pd_xge2_6:: MQPollDevice(xge2, QUEUE 6, BURST $BURST, PROMISC true) -> c2; 
pd_xge2_7:: MQPollDevice(xge2, QUEUE 7, BURST $BURST, PROMISC true) -> c2; 
pd_xge2_8:: MQPollDevice(xge2, QUEUE 8, BURST $BURST, PROMISC true) -> c2; 
pd_xge2_9:: MQPollDevice(xge2, QUEUE 9, BURST $BURST, PROMISC true) -> c2; 
pd_xge2_10:: MQPollDevice(xge2, QUEUE 10, BURST $BURST, PROMISC true) -> c2; 
pd_xge2_11:: MQPollDevice(xge2, QUEUE 11, BURST $BURST, PROMISC true) -> c2; 
out2 :: IsoCPUQueue(200);
out2  ->tod_xge2_0 :: MQToDevice(xge2, QUEUE 0, BURST $BURST) 
out2  ->tod_xge2_1 :: MQToDevice(xge2, QUEUE 1, BURST $BURST) 
out2  ->tod_xge2_2 :: MQToDevice(xge2, QUEUE 2, BURST $BURST) 
out2  ->tod_xge2_3 :: MQToDevice(xge2, QUEUE 3, BURST $BURST) 
out2  ->tod_xge2_4 :: MQToDevice(xge2, QUEUE 4, BURST $BURST) 
out2  ->tod_xge2_5 :: MQToDevice(xge2, QUEUE 5, BURST $BURST) 
out2  ->tod_xge2_6 :: MQToDevice(xge2, QUEUE 6, BURST $BURST) 
out2  ->tod_xge2_7 :: MQToDevice(xge2, QUEUE 7, BURST $BURST) 
out2  ->tod_xge2_8 :: MQToDevice(xge2, QUEUE 8, BURST $BURST) 
out2  ->tod_xge2_9 :: MQToDevice(xge2, QUEUE 9, BURST $BURST) 
out2  ->tod_xge2_10 :: MQToDevice(xge2, QUEUE 10, BURST $BURST) 
out2  ->tod_xge2_11 :: MQToDevice(xge2, QUEUE 11, BURST $BURST) 

c2[0] -> Discard;
//c2[0] -> ar2 :: ARPResponder(10.0.2.1 xge2) -> out2;
//arpq2 :: ARPQuerier(10.0.2.1, xge2) -> out2;
//c2[1] -> arpt;
//arpt[2] -> [1]arpq2;
c2[1] -> Discard; //ARP response
c2[2] -> Paint(3) -> ip;
c2[3] -> Print("xge2 non-IP") -> Discard;



c3 :: Classifier(12/0806 20/0001, 12/0806 20/0002, 12/0800, -);

pd_xge3_0:: MQPollDevice(xge3, QUEUE 0, BURST $BURST, PROMISC true) -> c3; 
pd_xge3_1:: MQPollDevice(xge3, QUEUE 1, BURST $BURST, PROMISC true) -> c3; 
pd_xge3_2:: MQPollDevice(xge3, QUEUE 2, BURST $BURST, PROMISC true) -> c3; 
pd_xge3_3:: MQPollDevice(xge3, QUEUE 3, BURST $BURST, PROMISC true) -> c3; 
pd_xge3_4:: MQPollDevice(xge3, QUEUE 4, BURST $BURST, PROMISC true) -> c3; 
pd_xge3_5:: MQPollDevice(xge3, QUEUE 5, BURST $BURST, PROMISC true) -> c3; 
pd_xge3_6:: MQPollDevice(xge3, QUEUE 6, BURST $BURST, PROMISC true) -> c3; 
pd_xge3_7:: MQPollDevice(xge3, QUEUE 7, BURST $BURST, PROMISC true) -> c3; 
pd_xge3_8:: MQPollDevice(xge3, QUEUE 8, BURST $BURST, PROMISC true) -> c3; 
pd_xge3_9:: MQPollDevice(xge3, QUEUE 9, BURST $BURST, PROMISC true) -> c3; 
pd_xge3_10:: MQPollDevice(xge3, QUEUE 10, BURST $BURST, PROMISC true) -> c3; 
pd_xge3_11:: MQPollDevice(xge3, QUEUE 11, BURST $BURST, PROMISC 1) -> c3; 


out3 :: IsoCPUQueue(200);
out3  ->tod_xge3_0 :: MQToDevice(xge3, QUEUE 0, BURST $BURST) 
out3  ->tod_xge3_1 :: MQToDevice(xge3, QUEUE 1, BURST $BURST) 
out3  ->tod_xge3_2 :: MQToDevice(xge3, QUEUE 2, BURST $BURST) 
out3  ->tod_xge3_3 :: MQToDevice(xge3, QUEUE 3, BURST $BURST) 
out3  ->tod_xge3_4 :: MQToDevice(xge3, QUEUE 4, BURST $BURST) 
out3  ->tod_xge3_5 :: MQToDevice(xge3, QUEUE 5, BURST $BURST) 
out3  ->tod_xge3_6 :: MQToDevice(xge3, QUEUE 6, BURST $BURST) 
out3  ->tod_xge3_7 :: MQToDevice(xge3, QUEUE 7, BURST $BURST) 
out3  ->tod_xge3_8 :: MQToDevice(xge3, QUEUE 8, BURST $BURST) 
out3  ->tod_xge3_9 :: MQToDevice(xge3, QUEUE 9, BURST $BURST) 
out3  ->tod_xge3_10 :: MQToDevice(xge3, QUEUE 10, BURST $BURST) 
out3  ->tod_xge3_11 :: MQToDevice(xge3, QUEUE 11, BURST $BURST) 

c3[0] -> Discard;
//c3[0] -> ar3 :: ARPResponder(10.0.3.1 xge3) -> out3;
//arpq3 :: ARPQuerier(10.0.3.1, xge3) -> out3;
//c3[1] -> arpt;
//arpt[3] -> [1]arpq3;
c3[1] -> Discard; //ARP response
c3[2] -> Paint(4) -> ip;
c3[3] -> Print("xge3 non-IP") -> Discard;


StaticThreadSched(  pd_xge0_0 0,   tod_xge0_0 0,   pd_xge1_0 0,   tod_xge1_0 0,   pd_xge2_0 0,   tod_xge2_0 0,   pd_xge3_0 0,   tod_xge3_0 0)
StaticThreadSched(  pd_xge0_1 1,   tod_xge0_1 1,   pd_xge1_1 1,   tod_xge1_1 1,   pd_xge2_1 1,   tod_xge2_1 1,   pd_xge3_1 1,   tod_xge3_1 1)
StaticThreadSched(  pd_xge0_2 2,   tod_xge0_2 2,   pd_xge1_2 2,   tod_xge1_2 2,   pd_xge2_2 2,   tod_xge2_2 2,   pd_xge3_2 2,   tod_xge3_2 2);
StaticThreadSched(  pd_xge0_3 3,   tod_xge0_3 3,   pd_xge1_3 3,   tod_xge1_3 3,   pd_xge2_3 3,   tod_xge2_3 3,   pd_xge3_3 3,   tod_xge3_3 3);
StaticThreadSched(  pd_xge0_4 4,   tod_xge0_4 4,   pd_xge1_4 4,   tod_xge1_4 4,   pd_xge2_4 4,   tod_xge2_4 4,   pd_xge3_4 4,   tod_xge3_4 4);
StaticThreadSched(  pd_xge0_5 5,   tod_xge0_5 5,   pd_xge1_5 5,   tod_xge1_5 5,   pd_xge2_5 5,   tod_xge2_5 5,   pd_xge3_5 5,   tod_xge3_5 5);
StaticThreadSched(  pd_xge0_6 6,   tod_xge0_6 6,   pd_xge1_6 6,   tod_xge1_6 6,   pd_xge2_6 6,   tod_xge2_6 6,   pd_xge3_6 6,   tod_xge3_6 6);
StaticThreadSched(  pd_xge0_7 7,   tod_xge0_7 7,   pd_xge1_7 7,   tod_xge1_7 7,   pd_xge2_7 7,   tod_xge2_7 7,   pd_xge3_7 7,   tod_xge3_7 7);
StaticThreadSched(  pd_xge0_8 8,   tod_xge0_8 8,   pd_xge1_8 8,   tod_xge1_8 8,   pd_xge2_8 8,   tod_xge2_8 8,   pd_xge3_8 8,   tod_xge3_8 8);
StaticThreadSched(  pd_xge0_9 9,   tod_xge0_9 9,   pd_xge1_9 9,   tod_xge1_9 9,   pd_xge2_9 9,   tod_xge2_9 9,   pd_xge3_9 9,   tod_xge3_9 9);
StaticThreadSched(  pd_xge0_10 10, tod_xge0_10 10, pd_xge1_10 10, tod_xge1_10 10, pd_xge2_10 10, tod_xge2_10 10, pd_xge3_10 10, tod_xge3_10 10);
StaticThreadSched(  pd_xge0_11 11, tod_xge0_11 11, pd_xge1_11 11, tod_xge1_11 11, pd_xge2_11 11, tod_xge2_11 11, pd_xge3_11 11, tod_xge3_11 11);

// Local delivery
toh :: Counter;
//arpt[4] -> toh;
rt[0] -> EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2) -> toh;
toh->Discard;

// Forwarding path for xge0
rt[1] -> DropBroadcasts
//    -> cp0 :: PaintTee(1)
    -> gio0 :: IPGWOptions(10.0.0.1)
    //-> FixIPSrc(10.0.0.1)
    -> dt0 :: DecIPTTL
    -> fr0 :: IPFragmenter(1500)
    //-> [0]arpq0;
    -> EtherEncap(0x0800, 00:1:1:1:1:1, 0:2:2:2:2:2) 
    -> out0;
dt0[1] -> ICMPError(10.0.0.1, timeexceeded) -> IPPrint() -> rt;
fr0[1] -> ICMPError(10.0.0.1, unreachable, needfrag)-> IPPrint()  -> rt;
gio0[1] -> ICMPError(10.0.0.1, parameterproblem)-> IPPrint()  -> rt;
//cp0[1] -> ICMPError(10.0.0.1, redirect, host)-> IPPrint()  -> rt;

// Forwarding path for xge1
rt[2] -> DropBroadcasts
//    -> cp1 :: PaintTee(2)
    -> gio1 :: IPGWOptions(10.0.1.1)
    //-> FixIPSrc(10.0.1.1)
    -> dt1 :: DecIPTTL
    -> fr1 :: IPFragmenter(1500)
    //-> [0]arpq1;
    -> EtherEncap(0x0800, 00:1:1:1:1:1, 0:2:2:2:2:2) 
    -> out1;
dt1[1] -> ICMPError(10.0.1.1, timeexceeded) -> IPPrint()  -> rt;
fr1[1] -> ICMPError(10.0.1.1, unreachable, needfrag) -> IPPrint()  -> rt;
gio1[1] -> ICMPError(10.0.1.1, parameterproblem) -> IPPrint()  -> rt;
//cp1[1] -> ICMPError(10.0.1.1, redirect, host)  -> IPPrint() -> rt;

// Forwarding path for xge2
rt[3] -> DropBroadcasts
//    -> cp2 :: PaintTee(3)
    -> gio2 :: IPGWOptions(10.0.2.1)
    //-> FixIPSrc(10.0.2.1)
    -> dt2 :: DecIPTTL
    -> fr2 :: IPFragmenter(1500)
    //-> [0]arpq2;
    -> EtherEncap(0x0800, 00:1:1:1:1:1, 0:2:2:2:2:2) 
    -> out2;
dt2[1] -> ICMPError(10.0.2.1, timeexceeded) -> rt;
fr2[1] -> ICMPError(10.0.2.1, unreachable, needfrag) -> rt;
gio2[1] -> ICMPError(10.0.2.1, parameterproblem) -> rt;
//cp2[1] -> ICMPError(10.0.2.1, redirect, host) -> rt;

// Forwarding path for xge3
rt[4] -> DropBroadcasts
//    -> cp3 :: PaintTee(4)
    -> gio3 :: IPGWOptions(10.0.3.1)
    //-> FixIPSrc(10.0.3.1)
    -> dt3 :: DecIPTTL
    -> fr3 :: IPFragmenter(1500)
    //-> [0]arpq3;
    -> EtherEncap(0x0800, 00:1:1:1:1:1, 0:2:2:2:2:2) 
    -> out3;
dt3[1] -> ICMPError(10.0.3.1, timeexceeded) -> rt;
fr3[1] -> ICMPError(10.0.3.1, unreachable, needfrag) -> rt;
gio3[1] -> ICMPError(10.0.3.1, parameterproblem) -> rt;
//cp3[1] -> ICMPError(10.0.3.1, redirect, host) -> rt;

Script(write rt.load /home/dongsuh/xia-core/click-2.0/conf/xia/ip_routes_4_bal.txt);

ControlSocket("TCP", 5600)

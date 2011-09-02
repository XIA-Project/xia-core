#!/usr/local/sbin/click-install -uct4

pd_eth2_0:: MQPollDevice(eth2, QUEUE 0, BURST 32, PROMISC true) -> Discard; 
pd_eth2_1:: MQPollDevice(eth2, QUEUE 1, BURST 32, PROMISC true) -> Discard; 
//pd_eth2_2:: MQPollDevice(eth2, QUEUE 2, BURST 32, PROMISC true) -> Discard; 
//pd_eth2_3:: MQPollDevice(eth2, QUEUE 3, BURST 32, PROMISC true) -> Discard; 
//pd_eth2_4:: MQPollDevice(eth2, QUEUE 4, BURST 32, PROMISC true) -> Discard; 
//pd_eth2_5:: MQPollDevice(eth2, QUEUE 5, BURST 32, PROMISC true) -> Discard; 
//pd_eth2_6:: MQPollDevice(eth2, QUEUE 6, BURST 32, PROMISC true) -> Discard; 
//pd_eth2_7:: MQPollDevice(eth2, QUEUE 7, BURST 32, PROMISC true) -> Discard; 
//pd_eth2_8:: MQPollDevice(eth2, QUEUE 8, BURST 32, PROMISC true) -> Discard; 
//pd_eth2_9:: MQPollDevice(eth2, QUEUE 9, BURST 32, PROMISC true) -> Discard; 
//pd_eth2_10:: MQPollDevice(eth2, QUEUE 10, BURST 32, PROMISC true) -> Discard; 
//pd_eth2_11:: MQPollDevice(eth2, QUEUE 11, BURST 32, PROMISC true) -> Discard; 
//pd_eth2_12:: MQPollDevice(eth2, QUEUE 12, BURST 32, PROMISC true) -> Discard; 
//pd_eth2_13:: MQPollDevice(eth2, QUEUE 13, BURST 32, PROMISC true) -> Discard; 
//pd_eth2_14:: MQPollDevice(eth2, QUEUE 14, BURST 32, PROMISC true) -> Discard; 
//pd_eth2_15:: MQPollDevice(eth2, QUEUE 15, BURST 32, PROMISC true) -> Discard; 
//pd_eth2_16:: MQPollDevice(eth2, QUEUE 16, BURST 32, PROMISC true) -> Discard; 
//pd_eth2_17:: MQPollDevice(eth2, QUEUE 17, BURST 32, PROMISC true) -> Discard; 
//pd_eth2_18:: MQPollDevice(eth2, QUEUE 18, BURST 32, PROMISC true) -> Discard; 
//pd_eth2_19:: MQPollDevice(eth2, QUEUE 19, BURST 32, PROMISC true) -> Discard; 
//pd_eth2_20:: MQPollDevice(eth2, QUEUE 20, BURST 32, PROMISC true) -> Discard; 
//pd_eth2_21:: MQPollDevice(eth2, QUEUE 21, BURST 32, PROMISC true) -> Discard; 
//pd_eth2_22:: MQPollDevice(eth2, QUEUE 22, BURST 32, PROMISC true) -> Discard; 
//pd_eth2_23:: MQPollDevice(eth2, QUEUE 23, BURST 32, PROMISC true) -> Discard; 

//pd_eth3_0:: MQPollDevice(eth3, QUEUE 0, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_1:: MQPollDevice(eth3, QUEUE 1, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_2:: MQPollDevice(eth3, QUEUE 2, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_3:: MQPollDevice(eth3, QUEUE 3, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_4:: MQPollDevice(eth3, QUEUE 4, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_5:: MQPollDevice(eth3, QUEUE 5, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_6:: MQPollDevice(eth3, QUEUE 6, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_7:: MQPollDevice(eth3, QUEUE 7, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_8:: MQPollDevice(eth3, QUEUE 8, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_9:: MQPollDevice(eth3, QUEUE 9, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_10:: MQPollDevice(eth3, QUEUE 10, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_11:: MQPollDevice(eth3, QUEUE 11, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_12:: MQPollDevice(eth3, QUEUE 12, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_13:: MQPollDevice(eth3, QUEUE 13, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_14:: MQPollDevice(eth3, QUEUE 14, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_15:: MQPollDevice(eth3, QUEUE 15, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_16:: MQPollDevice(eth3, QUEUE 16, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_17:: MQPollDevice(eth3, QUEUE 17, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_18:: MQPollDevice(eth3, QUEUE 18, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_19:: MQPollDevice(eth3, QUEUE 19, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_20:: MQPollDevice(eth3, QUEUE 20, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_21:: MQPollDevice(eth3, QUEUE 21, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_22:: MQPollDevice(eth3, QUEUE 22, BURST 32, PROMISC true) -> Discard; 
//pd_eth3_23:: MQPollDevice(eth3, QUEUE 23, BURST 32, PROMISC true) -> Discard; 

//pd_eth4_0:: MQPollDevice(eth4, QUEUE 0, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_1:: MQPollDevice(eth4, QUEUE 1, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_2:: MQPollDevice(eth4, QUEUE 2, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_3:: MQPollDevice(eth4, QUEUE 3, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_4:: MQPollDevice(eth4, QUEUE 4, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_5:: MQPollDevice(eth4, QUEUE 5, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_6:: MQPollDevice(eth4, QUEUE 6, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_7:: MQPollDevice(eth4, QUEUE 7, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_8:: MQPollDevice(eth4, QUEUE 8, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_9:: MQPollDevice(eth4, QUEUE 9, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_10:: MQPollDevice(eth4, QUEUE 10, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_11:: MQPollDevice(eth4, QUEUE 11, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_12:: MQPollDevice(eth4, QUEUE 12, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_13:: MQPollDevice(eth4, QUEUE 13, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_14:: MQPollDevice(eth4, QUEUE 14, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_15:: MQPollDevice(eth4, QUEUE 15, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_16:: MQPollDevice(eth4, QUEUE 16, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_17:: MQPollDevice(eth4, QUEUE 17, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_18:: MQPollDevice(eth4, QUEUE 18, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_19:: MQPollDevice(eth4, QUEUE 19, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_20:: MQPollDevice(eth4, QUEUE 20, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_21:: MQPollDevice(eth4, QUEUE 21, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_22:: MQPollDevice(eth4, QUEUE 22, BURST 32, PROMISC true) -> Discard; 
//pd_eth4_23:: MQPollDevice(eth4, QUEUE 23, BURST 32, PROMISC true) -> Discard; 

//pd_eth5_0:: MQPollDevice(eth5, QUEUE 0, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_1:: MQPollDevice(eth5, QUEUE 1, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_2:: MQPollDevice(eth5, QUEUE 2, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_3:: MQPollDevice(eth5, QUEUE 3, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_4:: MQPollDevice(eth5, QUEUE 4, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_5:: MQPollDevice(eth5, QUEUE 5, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_6:: MQPollDevice(eth5, QUEUE 6, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_7:: MQPollDevice(eth5, QUEUE 7, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_8:: MQPollDevice(eth5, QUEUE 8, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_9:: MQPollDevice(eth5, QUEUE 9, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_10:: MQPollDevice(eth5, QUEUE 10, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_11:: MQPollDevice(eth5, QUEUE 11, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_12:: MQPollDevice(eth5, QUEUE 12, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_13:: MQPollDevice(eth5, QUEUE 13, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_14:: MQPollDevice(eth5, QUEUE 14, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_15:: MQPollDevice(eth5, QUEUE 15, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_16:: MQPollDevice(eth5, QUEUE 16, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_17:: MQPollDevice(eth5, QUEUE 17, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_18:: MQPollDevice(eth5, QUEUE 18, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_19:: MQPollDevice(eth5, QUEUE 19, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_20:: MQPollDevice(eth5, QUEUE 20, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_21:: MQPollDevice(eth5, QUEUE 21, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_22:: MQPollDevice(eth5, QUEUE 22, BURST 32, PROMISC true) -> Discard; 
//pd_eth5_23:: MQPollDevice(eth5, QUEUE 23, BURST 32, PROMISC true) -> Discard; 

StaticThreadSched(
pd_eth2_0 0,
pd_eth2_1 2,
//pd_eth2_2 2,
//pd_eth2_3 3,
//pd_eth2_4 4,
//pd_eth2_5 5,
//pd_eth2_6 6,
//pd_eth2_7 7,
//pd_eth2_8 8,
//pd_eth2_9 9,
//pd_eth2_10 10,
//pd_eth2_11 11,
//pd_eth2_12 12,
//pd_eth2_13 13,
//pd_eth2_14 14,
//pd_eth2_15 15,
//pd_eth2_16 16,
//pd_eth2_17 17,
//pd_eth2_18 18,
//pd_eth2_19 19,
//pd_eth2_20 20,
//pd_eth2_21 21,
//pd_eth2_22 22,
//pd_eth2_23 23,
//
//pd_eth3_0 0,
//pd_eth3_1 1,
//pd_eth3_2 2,
//pd_eth3_3 3,
//pd_eth3_4 4,
//pd_eth3_5 5,
//pd_eth3_6 6,
//pd_eth3_7 7,
//pd_eth3_8 8,
//pd_eth3_9 9,
//pd_eth3_10 10,
//pd_eth3_11 11,
//pd_eth3_12 12,
//pd_eth3_13 13,
//pd_eth3_14 14,
//pd_eth3_15 15,
//pd_eth3_16 16,
//pd_eth3_17 17,
//pd_eth3_18 18,
//pd_eth3_19 19,
//pd_eth3_20 20,
//pd_eth3_21 21,
//pd_eth3_22 22,
//pd_eth3_23 23,
//
//pd_eth4_0 0,
//pd_eth4_1 1,
//pd_eth4_2 2,
//pd_eth4_3 3,
//pd_eth4_4 4,
//pd_eth4_5 5,
//pd_eth4_6 6,
//pd_eth4_7 7,
//pd_eth4_8 8,
//pd_eth4_9 9,
//pd_eth4_10 10,
//pd_eth4_11 11,
//pd_eth4_12 12,
//pd_eth4_13 13,
//pd_eth4_14 14,
//pd_eth4_15 15,
//pd_eth4_16 16,
//pd_eth4_17 17,
//pd_eth4_18 18,
//pd_eth4_19 19,
//pd_eth4_20 20,
//pd_eth4_21 21,
//pd_eth4_22 22,
//pd_eth4_23 23,
//
//pd_eth5_0 0,
//pd_eth5_1 1,
//pd_eth5_2 2,
//pd_eth5_3 3,
//pd_eth5_4 4,
//pd_eth5_5 5,
//pd_eth5_6 6,
//pd_eth5_7 7,
//pd_eth5_8 8,
//pd_eth5_9 9,
//pd_eth5_10 10, 
//pd_eth5_11 11,
//pd_eth5_12 12,
//pd_eth5_13 13,
//pd_eth5_14 14,
//pd_eth5_15 15,
//pd_eth5_16 16,
//pd_eth5_17 17,
//pd_eth5_18 18,
//pd_eth5_19 19,
//pd_eth5_20 20,
//pd_eth5_21 21,
//pd_eth5_22 22,
//pd_eth5_23 23
 )

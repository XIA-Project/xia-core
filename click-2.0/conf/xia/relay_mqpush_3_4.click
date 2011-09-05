#!/usr/local/sbin/click-install -uct12

define ($RX_BURST 32);
define ($TX_BURST 32);

pd_eth2_0:: MQPollDevice(eth2, QUEUE 0, BURST $RX_BURST, PROMISC true)  -> 
   td_eth2_0::MQPushToDevice(eth2, QUEUE 0, BURST $TX_BURST);
pd_eth2_1:: MQPollDevice(eth2, QUEUE 1, BURST $RX_BURST, PROMISC true)  ->
   td_eth2_1::MQPushToDevice(eth2, QUEUE 1, BURST $TX_BURST);
pd_eth2_2:: MQPollDevice(eth2, QUEUE 2, BURST $RX_BURST, PROMISC true)  ->
   td_eth2_2::MQPushToDevice(eth2, QUEUE 2, BURST $TX_BURST);

pd_eth3_0:: MQPollDevice(eth3, QUEUE 0, BURST $RX_BURST, PROMISC true) ->
   td_eth3_0::MQPushToDevice(eth3, QUEUE 0, BURST $TX_BURST);
pd_eth3_1:: MQPollDevice(eth3, QUEUE 1, BURST $RX_BURST, PROMISC true) ->
   td_eth3_1::MQPushToDevice(eth3, QUEUE 1, BURST $TX_BURST);
pd_eth3_2:: MQPollDevice(eth3, QUEUE 2, BURST $RX_BURST, PROMISC true) ->
   td_eth3_2::MQPushToDevice(eth3, QUEUE 2, BURST $TX_BURST);

pd_eth4_0:: MQPollDevice(eth4, QUEUE 0, BURST $RX_BURST, PROMISC true) ->
   td_eth4_0::MQPushToDevice(eth4, QUEUE 0, BURST $TX_BURST);
pd_eth4_1:: MQPollDevice(eth4, QUEUE 1, BURST $RX_BURST, PROMISC true) ->
   td_eth4_1::MQPushToDevice(eth4, QUEUE 1, BURST $TX_BURST);
pd_eth4_2:: MQPollDevice(eth4, QUEUE 2, BURST $RX_BURST, PROMISC true) ->
   td_eth4_2::MQPushToDevice(eth4, QUEUE 2, BURST $TX_BURST);

pd_eth5_0:: MQPollDevice(eth5, QUEUE 0, BURST $RX_BURST, PROMISC true) ->
   td_eth5_0::MQPushToDevice(eth5, QUEUE 0, BURST $TX_BURST);
pd_eth5_1:: MQPollDevice(eth5, QUEUE 1, BURST $RX_BURST, PROMISC true) ->
   td_eth5_1::MQPushToDevice(eth5, QUEUE 1, BURST $TX_BURST);
pd_eth5_2:: MQPollDevice(eth5, QUEUE 2, BURST $RX_BURST, PROMISC true) ->
   td_eth5_2::MQPushToDevice(eth5, QUEUE 2, BURST $TX_BURST);

StaticThreadSched(
pd_eth2_0 0,
pd_eth2_1 2,
pd_eth2_2 4,
pd_eth3_0 6,
pd_eth3_1 8,
pd_eth3_2 10,
pd_eth4_0 1,
pd_eth4_1 3,
pd_eth4_2 5,
pd_eth5_0 7,
pd_eth5_1 9,
pd_eth5_2 11,
 )

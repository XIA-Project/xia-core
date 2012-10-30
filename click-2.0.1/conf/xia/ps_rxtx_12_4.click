define ($RX_BURST 64);
define ($TX_BURST 64);
define ($QSIZE 256);

elementclass relay {
    $eth, $queue, $cpu |

    fd :: PSFromDevice($eth, QUEUE $queue, BURST $RX_BURST)
    -> SimpleQueue($QSIZE) ->
    pd :: PSToDevice($eth, QUEUE $queue, BURST $TX_BURST);

    StaticThreadSched(fd $cpu, pd $cpu);
}

elementclass relay_array {
    $eth |
    relay($eth, 0, 0);
    relay($eth, 1, 1);
    relay($eth, 2, 2);
    relay($eth, 3, 3);
    relay($eth, 4, 4);
    relay($eth, 5, 5);
    relay($eth, 6, 6);
    relay($eth, 7, 7);
    relay($eth, 8, 8);
    relay($eth, 9, 9);
    relay($eth, 10, 10);
    relay($eth, 11, 11);
}

relay_array(xge0);
relay_array(xge1);
relay_array(xge2);
relay_array(xge3);


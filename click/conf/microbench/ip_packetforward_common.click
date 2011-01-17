
elementclass IPPacketForward {
    host0 :: IPRouter4Port(192.168.0.1);

    Script(write host0/rt.add 192.168.0.0/24 0);
    Script(write host0/rt.add 0.0.0.0/0.0.0.0 1);

    input -> Clone($COUNT)
    -> Unqueue
    -> host0 
    -> Unqueue
    -> AggregateCounter(COUNT_STOP $COUNT)
    -> Discard;

    Idle -> [1]host0;
    Idle -> [2]host0;
    Idle -> [3]host0;
    host0[1] -> Discard;
    host0[2] -> Discard;
    host0[3] -> Discard;
};


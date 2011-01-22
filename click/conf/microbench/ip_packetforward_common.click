
elementclass IPPacketForward {
    host0 :: IPRouter4Port(192.168.0.1);

    //Script(write host0/rt.add 192.168.0.0/24 0);
    //Script(write host0/rt.add 0.0.0.0/0.0.0.0 1);
    Script(write host0/rt.load ip_routes.txt);

    input -> Clone($COUNT)
    -> Unqueue
    -> IPRandomize(MAX_CYCLE $RT_SIZE)
    //-> Print
    -> host0 
    -> Unqueue
    -> PrintStats
    -> AggregateCounter(COUNT_STOP $COUNT)
    -> Discard;

    input[1] -> IPRandomize(MAX_CYCLE $RT_SIZE) -> host0;

    Idle -> [1]host0;
    Idle -> [2]host0;
    Idle -> [3]host0;
    host0[1] -> Discard;
    host0[2] -> Discard;
    host0[3] -> Discard;
};


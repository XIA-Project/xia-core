
elementclass XIAPacketForward {
    host0 :: Router4Port(RE AD0);

    Script(write host0/n/proc/rt_AD/rt.add - 5);
    Script(write host0/n/proc/rt_HID/rt.add - 5);
    Script(write host0/n/proc/rt_SID/rt.add - 5);
    Script(write host0/n/proc/rt_CID/rt.add - 5);

    Script(write host0/n/proc/rt_AD/rt.add AD0 4);
    Script(write host0/n/proc/rt_AD/rt.add AD1 0);  // the only forwardable XID for this node

    input -> Clone($COUNT)
    -> Unqueue
    -> host0 
    -> Unqueue
    -> AggregateCounter(COUNT_STOP $COUNT)
    -> Discard;

    input[1] -> host0;

    Idle -> [1]host0;
    Idle -> [2]host0;
    Idle -> [3]host0;
    host0[1] -> Discard;
    host0[2] -> Discard;
    host0[3] -> Discard;
};



elementclass XIAPacketForward {
    host0 :: Router4PortDummyCache(RE SELF_AD);

    Script(write host0/n/proc/rt_AD/rt.add - 5);
    Script(write host0/n/proc/rt_HID/rt.add - 5);
    Script(write host0/n/proc/rt_SID/rt.add - 5);
    Script(write host0/n/proc/rt_CID/rt.add - 5);

    Script(write host0/n/proc/rt_AD/rt.add SELF_AD 4);
    Script(write host0/n/proc/rt_AD/rt.generate AD $RT_SIZE 0);

    input -> Clone($COUNT)
    -> Unqueue
    -> XIARandomize(XID_TYPE AD, MAX_CYCLE $RT_SIZE)
    //-> XIAPrint
    -> host0 
    -> Unqueue
    -> PrintStats
    -> AggregateCounter(COUNT_STOP $COUNT)
    -> Discard;

    input[1] -> XIARandomize(XID_TYPE AD, MAX_CYCLE $RT_SIZE) -> host0;

    Idle -> [1]host0;
    Idle -> [2]host0;
    Idle -> [3]host0;
    host0[1] -> Discard;
    host0[2] -> Discard;
    host0[3] -> Discard;
};


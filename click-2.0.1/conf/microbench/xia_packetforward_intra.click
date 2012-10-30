require(library ../xia_router_template.inc);
require(library common.inc);

//define($AD_RT_SIZE 351611);
define($AD_RT_SIZE 1);
define($MAX_CYCLE $AD_RT_SIZE);
define($COUNT2 100000);
define($CID_RT_SIZE 10);
define ($QUEUESIZE 50000)

elementclass Intra {
    $clone2|

elementclass SingleLookup { 
    $clone|
    c :: XIAXIDTypeClassifier(next AD, next HID, next SID, next CID, -);
    input -> c  

    //  Next destination is AD
    c[0] -> rt_AD :: GenericRouting4Port;
    rt_AD[0] -> [0]output;
    rt_AD[1] -> XIAPrint(error) -> Discard
    rt_AD[2] -> [0]output;

    //  Next destination is HID
    c[1] -> rt_HID :: GenericRouting4Port;
    rt_HID[0] -> [0]output;
    rt_HID[1] -> XIAPrint(error) -> Discard
    rt_HID[2] -> [0]output;

    //  Next destination is SID
    c[2] -> rt_SID :: GenericRouting4Port;
    rt_SID[0] -> [0]output;
    rt_SID[1] -> XIAPrint(error) -> Discard
    rt_SID[2] -> [0]output;


    //  Next destination is CID
    c[3] -> rt_CID :: GenericRouting4Port;
    rt_CID[0] -> [0]output;
    rt_CID[1] -> XIAPrint(error) -> Discard
    rt_CID[2] -> [0]output;

    c[4] -> [0]output ;

    Script(write rt_AD/rt.add - 5);
    Script(write rt_HID/rt.add - 5);
    Script(write rt_SID/rt.add - 5);
    Script(write rt_CID/rt.add - 5);

    Script(write rt_AD/rt.add SELF_AD 4);
    Script(write $clone.active true );
    Script(print_usertime, write rt_AD/rt.generate AD $AD_RT_SIZE 0, print_usertime);
}

input
-> SimpleQueue
-> RatedUnqueue(10000)
-> Paint(10)
//-> cl::Clone($COUNT2, ACTIVE false)
-> XIARandomize(XID_TYPE AD, MAX_CYCLE $MAX_CYCLE)
//-> u::Unqueue()
-> t::Tee(4)

t[0]-> SimpleQueue($QUEUESIZE) -> u0::Unqueue() -> Strip(64)-> Unstrip(64) -> SetTimestamp -> Paint(0,17)-> x0::XIASelectPath(first) ->  lookup::SingleLookup($clone2)
t[1]-> SimpleQueue($QUEUESIZE) -> u1::Unqueue() -> Strip(64)-> Unstrip(64) -> SetTimestamp -> Paint(1,17)-> x1::XIASelectPath(first) ->  x4::XIASelectPath(next)-> lookup
t[2]-> SimpleQueue($QUEUESIZE) -> u2::Unqueue() -> Strip(64)-> Unstrip(64) -> SetTimestamp -> Paint(2,17)-> x2::XIASelectPath(first) ->  x5::XIASelectPath(next)-> x7::XIASelectPath(next)-> lookup 
t[3]-> SimpleQueue($QUEUESIZE) -> u3::Unqueue() -> Strip(64)-> Unstrip(64) -> SetTimestamp -> Paint(3,17)-> x3::XIASelectPath(first) ->  x6::XIASelectPath(next)-> x8::XIASelectPath(next)-> x9::XIASelectPath(next)-> lookup 

lookup[0]-> p0::PaintSwitch(17) 
//lookup[1]-> p1::PaintSwitch(17)
x0[1]-> p0
x1[1]-> p0
x2[1]-> p0
x3[1]-> p0
x4[1]-> p0
x5[1]-> p0
x6[1]-> p0
x7[1]-> p0
x8[1]-> p0
x9[1]-> p0
   
p0[0]->TimestampAccum-> [0]b::IsoCPUPush(50000); //q0::Queue($QUEUESIZE)
p0[1]->TimestampAccum-> [1]b; //q1::Queue($QUEUESIZE)
p0[2]->TimestampAccum-> [2]b; //q2::Queue($QUEUESIZE)
p0[3]->TimestampAccum-> [3]b; //q3::Queue($QUEUESIZE)

//p1[0]->XIAPrint(One)->TimestampAccum->[0]b; //q0
//p1[1]->XIAPrint(Two)->TimestampAccum->[1]b; //q1
//p1[2]->XIAPrint(Thr)->TimestampAccum->[2]b; //q2
//p1[3]->XIAPrint(Fou)->TimestampAccum->[3]b; //q3

//q0-> [0]b::Barrier()
//q1-> [1]b
//q2-> [2]b
//q3-> [3]b

b-> TimestampAccum -> XIADecHLIM ->TimestampAccum -> AggregateCounter(COUNT_STOP $COUNT2, PRINT_USERTIME true)  -> Discard()

//StaticThreadSched(u 0, b 0, u0 0, gen 0, u1 2, u2 4, u3 6)
StaticThreadSched(b 0, u0 0, gen 2, u1 4, u2 6, u3 8)

}

elementclass XIAPacketRouteBench {
    $local_addr |

    // $local_addr: the full address of the node (only used for debugging)

    // input: a packet to process
    // output[0]: forward (painted)
    // output[1]: arrived at destination node
    // output[2]: could not route at all (tried all paths)

    check_dest :: XIACheckDest();
    consider_first_path :: XIASelectPath(first);
    consider_next_path :: XIASelectPath(next);
    c :: XIAXIDTypeClassifier(next AD, next HID, next SID, next CID, -);

    //input -> Print("packet received by $local_addr") -> consider_first_path;
    input -> consider_first_path;

    check_dest[0] -> [1]output;             // arrived at the final destination
    check_dest[1] -> consider_first_path;   // reiterate paths with new last pointer

    consider_first_path[0] -> c;
    consider_first_path[1] -> [2]output;
    consider_next_path[0] -> c;
    consider_next_path[1] -> [2]output;

    //  Next destination is AD
    c[0] ->rt_AD :: GenericRouting4Port;
    rt_AD[0] -> GenericPostRouteProc -> [0]output;
    rt_AD[1] -> XIANextHop -> check_dest;
    rt_AD[2] -> consider_next_path;

    //  Next destination is HID
    c[1] -> rt_HID :: GenericRouting4Port;
    rt_HID[0] -> GenericPostRouteProc -> [0]output;
    rt_HID[1] -> XIANextHop -> check_dest;
    rt_HID[2] -> consider_next_path;

    //  Next destination is SID
    c[2] -> rt_SID :: GenericRouting4Port;
    rt_SID[0] -> GenericPostRouteProc -> [0]output;
    rt_SID[1] -> XIANextHop -> check_dest;
    rt_SID[2] -> consider_next_path;


    // change this if you want to do CID post route processing for any reason
    CIDPostRouteProc :: Null;

    //  Next destination is CID
    c[3] -> rt_CID :: GenericRouting4Port;
    rt_CID[0] -> GenericPostRouteProc -> CIDPostRouteProc -> [0]output;
    rt_CID[1] -> XIANextHop -> check_dest;
    rt_CID[2] -> consider_next_path;

    c[4] -> [2]output;
};

elementclass SerialPath {
input
-> RatedUnqueue(10000)
-> XIARandomize(XID_TYPE AD, MAX_CYCLE $AD_RANDOMIZE_MAX_CYCLE)
-> Strip(64)
-> Unstrip(64)
-> SetTimestamp()
-> proc::XIAPacketRoute(RE SELF_AD HID)

Script(print_usertime, write proc/rt_AD/rt.generate AD $AD_RT_SIZE 0, print_usertime);
Script(write proc/rt_AD/rt.add - 5);
Script(write proc/rt_HID/rt.add - 5);
Script(write proc/rt_SID/rt.add - 5);
Script(write proc/rt_CID/rt.add - 5);

proc[0]-> TimestampAccum() -> AggregateCounter(COUNT_STOP $COUNT2, PRINT_USERTIME true)  -> Discard()
proc[1]-> XIAPrint(error) -> Discard
proc[2]-> XIAPrint(error) ->Discard
}

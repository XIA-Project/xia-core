require(library ../xia_router_template.inc);
require(library common.inc);

define($AD_RT_SIZE 351611);
define($MAX_CYCLE $AD_RT_SIZE);
//define($COUNT 100000);
define($CID_RT_SIZE 10);
define ($QUEUESIZE 50000)

elementclass SingleLookup { 
    $clone|
    c :: XIAXIDTypeClassifier(next AD, next HID, next SID, next CID, -);
    input -> c  

    //  Next destination is AD
    c[0] -> rt_AD :: GenericRouting4Port;
    rt_AD[0] -> [0]output;
    rt_AD[1] -> XIAPrint(error) -> Discard
    rt_AD[2] -> [1]output;

    //  Next destination is HID
    c[1] -> rt_HID :: GenericRouting4Port;
    rt_HID[0] -> [0]output;
    rt_HID[1] -> XIAPrint(error) -> Discard
    rt_HID[2] -> [1]output;

    //  Next destination is SID
    c[2] -> rt_SID :: GenericRouting4Port;
    rt_SID[0] -> [0]output;
    rt_SID[1] -> XIAPrint(error) -> Discard
    rt_SID[2] -> [1]output;


    //  Next destination is CID
    c[3] -> rt_CID :: GenericRouting4Port;
    rt_CID[0] -> [0]output;
    rt_CID[1] -> XIAPrint(error) -> Discard
    rt_CID[2] -> [1]output;

    c[4] -> [1]output ;

    Script(write rt_AD/rt.add - 5);
    Script(write rt_HID/rt.add - 5);
    Script(write rt_SID/rt.add - 5);
    Script(write rt_CID/rt.add - 5);

    Script(write rt_AD/rt.add SELF_AD 4);
    Script(write $clone.active true );
    Script(print_usertime, write rt_AD/rt.generate AD $AD_RT_SIZE 0, print_usertime);
}

gen :: InfiniteSource(LENGTH $PAYLOAD_SIZE_XIA_XID2, ACTIVE false, HEADROOM $HEADROOM_SIZE_XIA_XID2)
-> Script(TYPE PACKET, write gen.active false)       // stop source after exactly 1 packet
-> XIAEncap(SRC RE UNROUTABLE_AD0, DST RE RANDOM_ID)
-> Paint(10)
-> cl::Clone($COUNT, ACTIVE false)
-> XIARandomize(XID_TYPE AD, MAX_CYCLE $MAX_CYCLE)
-> u::Unqueue()
-> t::Tee(4)

t[0]-> Queue($QUEUESIZE) -> u0::Unqueue() -> Paint(0,17)-> x0::XIASelectPath(first) ->  lookup::SingleLookup(cl)
t[1]-> Queue($QUEUESIZE) -> u1::Unqueue() -> Paint(1,17)-> x1::XIASelectPath(first) ->  x4::XIASelectPath(next)-> lookup
t[2]-> Queue($QUEUESIZE) -> u2::Unqueue() -> Paint(2,17)-> x2::XIASelectPath(first) ->  x5::XIASelectPath(next)-> x7::XIASelectPath(next)-> lookup 
t[3]-> Queue($QUEUESIZE) -> u3::Unqueue() -> Paint(3,17)-> x3::XIASelectPath(first) ->  x6::XIASelectPath(next)-> x8::XIASelectPath(next)-> x9::XIASelectPath(next)-> lookup 

lookup[0]-> p0::PaintSwitch(17) 
lookup[1]-> p1::PaintSwitch(17)
x0[1]-> p1
x1[1]-> p1
x2[1]-> p1
x3[1]-> p1
x4[1]-> p1
x5[1]-> p1
x6[1]-> p1
x7[1]-> p1
x8[1]-> p1
x9[1]-> p1
   
p0[0]-> q0::Queue($QUEUESIZE)
p0[1]-> q1::Queue($QUEUESIZE)
p0[2]-> q2::Queue($QUEUESIZE)
p0[3]-> q3::Queue($QUEUESIZE)

p1[0]->q0
p1[1]->q1
p1[2]->q2
p1[3]->q3

q0-> [0]b::Barrier()
q1-> [1]b
q2-> [2]b
q3-> [3]b

b-> XIADecHLIM -> AggregateCounter(COUNT_STOP $COUNT, PRINT_USERTIME true)  -> Discard()

StaticThreadSched(u 0, b 0, gen 0, u0 0, u1 1, u2 2, u3 3)

Script(write gen.active true);


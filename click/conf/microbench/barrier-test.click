
gen :: InfiniteSource(LENGTH 64, ACTIVE true, HEADROOM 64)
-> IPEncap(9, 10.0.0.1, 10.0.0.2)
-> IPPrint(gen)
-> t::Tee(3)

t[0]-> q0::Queue() -> i0::Unqueue() -> qq0::Queue()
t[1]-> q1::Queue() -> i1::Unqueue() -> qq1::Queue()
t[2]-> q2::Queue() -> i2::Unqueue() -> qq2::Queue()

b::Barrier()
qq0 -> [0]b
qq1 -> [1]b
qq2 -> [2]b

b -> IPPrint("out") ->Discard();

StaticThreadSched(i0 0, i1 1, i2 2);
//StaticThreadSched(i0 0);

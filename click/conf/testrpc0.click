sock0::Socket(TCP, 0.0.0.0,2000, CLIENT false);
sock1::Socket(TCP, 0.0.0.0,2001, CLIENT false);
sock2::Socket(TCP, 0.0.0.0,2002, CLIENT false);
localHost_out:: Print(CONTENTS 'ASCII') -> Discard;  //Replace Print for HID0
localHost_in0::TimedSource(INTERVAL 10, DATA "Application0 Request Served!") //Replace TimeSource for HID0
localHost_in1::TimedSource(INTERVAL 10, DATA "Application1 Request Served!") //Replace TimeSource for HID0
localHost_in2::TimedSource(INTERVAL 10, DATA "Application2 Request Served!") //Replace TimeSource for HID0
rpc0::rpc();

sock0 -> [0] rpc0;
rpc0[0] -> localHost_out;
localHost_in0 -> [1]rpc0;
rpc0[1]->sock0;
sock1 -> [2] rpc0;
rpc0[2] -> localHost_out;
localHost_in1 -> [3]rpc0;
rpc0[3]->sock1;
sock2 -> [4] rpc0;
rpc0[4] -> localHost_out;
localHost_in2 -> [5]rpc0;
rpc0[5]->sock2;

elementclass LocalhostSocket {
	$click_port |
	input -> Socket("UDP", 127.0.0.1, $click_port, SNAPLEN 65536) -> output;
};

elementclass DomainSocket {
	$click_port |
	input -> Socket("UNIX_DGRAM", "/tmp/api1-click.sock", SNAPLEN 65536) -> output;
};

ls :: LocalhostSocket(7654);
ds :: DomainSocket(0);

ls-> Queue()-> Print("echo: ", -1, CONTENTS ASCII) ->ls;
ds-> Queue()-> Print("echo: ", -1, CONTENTS ASCII) ->ds;


require(library ../../../click/conf/xia_router_lib.click);
require(library xia_address.click);

// router instantiation
xiademo5 :: XIARouter4Port(RE AD:95bb3bbb81cf26468231b716584ce4592c2ef9f8 HID:4dc3b89f82fe272e0b9e48e5234df4a7e6352326, AD:95bb3bbb81cf26468231b716584ce4592c2ef9f8, HID:4dc3b89f82fe272e0b9e48e5234df4a7e6352326, 0.0.0.0, 1500, 74:d4:35:e6:6f:f7, 74:d4:35:e6:6f:f5, 00:50:b6:4f:b2:97, 80:86:f2:53:20:a5);


FromDevice(eth0, METHOD LINUX) -> [0]xiademo5[0] -> ToDevice(eth0)

FromDevice(eth1, METHOD LINUX) -> [1]xiademo5[1] -> ToDevice(eth1)

FromDevice(eth2, METHOD LINUX) -> [2]xiademo5[2] -> ToDevice(eth2)

FromDevice(wlan0, METHOD LINUX) -> [3]xiademo5[3] -> ToDevice(wlan0)


	ControlSocket(tcp, 7777);

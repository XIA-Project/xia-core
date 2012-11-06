require(library xia_router_template_xtransport.click);
require(library xia_address.click);

// host & router instantiation
//host0 :: EndHost (RE AD0 HID0, HID0, fake0,172.0.0.2,172.0.0.1,11:11:11:11:11:11,0);
//host1 :: EndHost (RE AD1 HID1, HID1, fake1,192.0.0.2,192.0.0.1,21:11:11:11:11:11,1);
//host0 :: Host(RE AD0 HID0, HID0, 2000);
//host1 :: Host(RE AD1 HID1, HID1, 2001);
host0 :: DHCPEndHost (HID0, fake0,172.0.0.2,172.0.0.1,11:11:11:11:11:11,0);
host1 :: DHCPEndHost (HID1, fake1,192.0.0.2,192.0.0.1,21:11:11:11:11:11,1);
router0 :: Router(RE AD0 RHID0, AD0, RHID0);
router1 :: Router(RE AD1 RHID1, AD1, RHID1);
hub0 :: Hub();
hub1 :: Hub();
dhcp0 :: XIADHCPServer(RE AD0 DHID0, RE BHID SID_DHCP, RE AD0);
dhcp1 :: XIADHCPServer(RE AD1 DHID1, RE BHID SID_DHCP, RE AD1);

// interconnection -- host - hub(dummy with DHCP) - ad
host0[0] ->  XIAPrint("host0->router0") -> LinkUnqueue(0.005, 1 GB/s) -> [0]hub0;
router0[0] -> XIAPrint("router0->host0") -> LinkUnqueue(0.005, 1 GB/s) -> [1]hub0;
hub0[0] -> [0]host0;
hub0[1] -> [0]router0;
dhcp0 -> TimedUnqueue(1) -> [2]hub0;
hub0[2] -> Discard();

host1[0] ->  XIAPrint("host1->router1") -> LinkUnqueue(0.005, 1 GB/s) -> [0]hub1;
router1[0] -> XIAPrint("router1->host1") -> LinkUnqueue(0.005, 1 GB/s) -> [1]hub1;
hub1[0] -> [0]host1;
hub1[1] -> [0]router1;
dhcp1 -> TimedUnqueue(1) -> [2]hub1;
hub1[2] -> Discard();

// interconnection -- ad - ad
router0[1] ->  XIAPrint("router0->router1") -> LinkUnqueue(0.005, 1 GB/s) ->[1]router1;
router1[1] ->  XIAPrint("router1->router0") -> LinkUnqueue(0.005, 1 GB/s) ->[1]router0;

ControlSocket(tcp, 7777);

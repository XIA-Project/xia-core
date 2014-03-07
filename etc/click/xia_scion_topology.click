require(library xia_router_lib.click);
require(library xia_address.click);

controller0 :: XIAController2Port(RE AD0 CHID0, AD0, CHID0, 0.0.0.0, 1500, aa:aa:aa:aa:aa:aa);
router0 :: XIARouter2Port(RE AD0 RHID0, AD0, RHID0, 0.0.0.0, 1600, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
beacon0 :: XIASCIONBeaconServerCore(RE AD0 BHID0, AD0, BHID0, 0.0.0.0, 1700, aa:aa:aa:aa:aa:aa,
        AID 11111,
        CONFIG_FILE "./TD1/TDC/AD1/beaconserver/conf/AD1BS.conf",
        TOPOLOGY_FILE "./TD1/TDC/AD1/topology1.xml",
        ROT "./TD1/TDC/AD1/beaconserver/ROT/rot-td1-0.xml");

controller1 :: XIAController2Port(RE AD1 CHID1, AD1, CHID1, 0.0.0.0, 1800, aa:aa:aa:aa:aa:aa);
router1 :: XIARouter4Port(RE AD1 RHID1, AD1, RHID1, 0.0.0.0, 1900,
        aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
beacon1 :: XIASCIONBeaconServer(RE AD1 BHID1, AD1, BHID1, 0.0.0.0, 2000, aa:aa:aa:aa:aa:aa,
        AID 22222,
        CONFIG_FILE "./TD1/Non-TDC/AD2/beaconserver/conf/AD2BS.conf",
        TOPOLOGY_FILE "./TD1/Non-TDC/AD2/topology2.xml",
        ROT "./TD1/Non-TDC/AD2/beaconserver/ROT/rot-td1-0.xml");

controller2 :: XIAController2Port(RE AD2 CHID2, AD2, CHID2, 0.0.0.0, 2100, aa:aa:aa:aa:aa:aa);
router2 :: XIARouter2Port(RE AD2 RHID2, AD2, RHID2, 0.0.0.0, 2200, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
beacon2 :: XIASCIONBeaconServer(RE AD2 BHID2, AD2, BHID2, 0.0.0.0, 2300, aa:aa:aa:aa:aa:aa,
        AID 33333,
        CONFIG_FILE "./TD1/Non-TDC/AD3/beaconserver/conf/AD3BS.conf",
        TOPOLOGY_FILE "./TD1/Non-TDC/AD3/topology3.xml",
        ROT "./TD1/Non-TDC/AD3/beaconserver/ROT/rot-td1-0.xml");

controller0[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]beacon0;
beacon0[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]controller0;

controller1[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]beacon1;
beacon1[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]controller1;

controller2[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]beacon2;
beacon2[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]controller2;


controller0[1] -> LinkUnqueue(0.005, 1 GB/s) -> [1]router0;
router0[1] -> LinkUnqueue(0.005, 1 GB/s) -> [1]controller0;

controller1[1] -> LinkUnqueue(0.005, 1 GB/s) -> [1]router1;
router1[1] -> LinkUnqueue(0.005, 1 GB/s) -> [1]controller1;

controller2[1] -> LinkUnqueue(0.005, 1 GB/s) -> [1]router2;
router2[1] -> LinkUnqueue(0.005, 1 GB/s) -> [1]controller2;


router0[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router1;
router1[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router0;

router2[0] -> LinkUnqueue(0.005, 1 GB/s) -> [2]router1;
router1[2] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router2;

Idle -> [3]router1;
router1[3] -> Idle;

// The following line is required by the xianet script so it can determine the appropriate
// host/router pair to run the nameserver on
// controller0 :: nameserver

ControlSocket(tcp, 7777);

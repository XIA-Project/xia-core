require(library xia_router_lib.click);
require(library xia_address.click);

controller1 :: XIAController4Port(RE AD1 CHID1, AD1, CHID1, 0.0.0.0, 1500, aa:aa:aa:aa:aa:aa);
router1 :: XIARouter2Port(RE AD1 RHID1, AD1, RHID1, 0.0.0.0, 1600, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
beacon1 :: XIASCIONBeaconServerCore(RE AD1 BHID1, AD1, BHID1, 0.0.0.0, 1700, aa:aa:aa:aa:aa:aa,
        AID 11111,
        CONFIG_FILE "./TD1/TDC/AD1/beaconserver/conf/AD1BS.conf",
        TOPOLOGY_FILE "./TD1/TDC/AD1/topology1.xml",
        ROT "./TD1/TDC/AD1/beaconserver/ROT/rot-td1-0.xml");
path1 :: XIASCIONPathServerCore(RE AD1 PHID1, AD1, PHID1, 0.0.0.0, 3700, aa:aa:aa:aa:aa:aa,
        AID 11111,
        CONFIG_FILE "./TD1/TDC/AD1/pathserver/conf/AD1PS.conf",
        TOPOLOGY_FILE "./TD1/TDC/AD1/topology1.xml");

controller2 :: XIAController4Port(RE AD2 CHID2, AD2, CHID2, 0.0.0.0, 1800, aa:aa:aa:aa:aa:aa);
router21 :: XIARouter4Port(RE AD2 RHID2, AD2, RHID2, 0.0.0.0, 1900,
        aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
router22 :: XIARouter4Port(RE AD2 RHID4, AD2, RHID4, 0.0.0.0, 2400, 
		aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
beacon2 :: XIASCIONBeaconServer(RE AD2 BHID2, AD2, BHID2, 0.0.0.0, 2000, aa:aa:aa:aa:aa:aa,
        AID 22222,
        CONFIG_FILE "./TD1/Non-TDC/AD2/beaconserver/conf/AD2BS.conf",
        TOPOLOGY_FILE "./TD1/Non-TDC/AD2/topology2.xml",
        ROT "./TD1/Non-TDC/AD2/beaconserver/ROT/rot-td1-0.xml");
path2 :: XIASCIONPathServer(RE AD2 PHID2, AD2, PHID2, 0.0.0.0, 3000, aa:aa:aa:aa:aa:aa,
        AID 22222,
        CONFIG_FILE "./TD1/Non-TDC/AD2/pathserver/conf/AD2PS.conf",
        TOPOLOGY_FILE "./TD1/Non-TDC/AD2/topology2.xml");

controller3 :: XIAController4Port(RE AD3 CHID3, AD3, CHID3, 0.0.0.0, 2100, aa:aa:aa:aa:aa:aa);
router3 :: XIARouter2Port(RE AD3 RHID3, AD3, RHID3, 0.0.0.0, 2200, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
beacon3 :: XIASCIONBeaconServer(RE AD3 BHID3, AD3, BHID3, 0.0.0.0, 2300, aa:aa:aa:aa:aa:aa,
        AID 33333,
        CONFIG_FILE "./TD1/Non-TDC/AD3/beaconserver/conf/AD3BS.conf",
        TOPOLOGY_FILE "./TD1/Non-TDC/AD3/topology3.xml",
        ROT "./TD1/Non-TDC/AD3/beaconserver/ROT/rot-td1-0.xml");
path3 :: XIASCIONPathServer(RE AD3 PHID3, AD3, PHID3, 0.0.0.0, 3300, aa:aa:aa:aa:aa:aa,
        AID 33333,
        CONFIG_FILE "./TD1/Non-TDC/AD3/pathserver/conf/AD3PS.conf",
        TOPOLOGY_FILE "./TD1/Non-TDC/AD3/topology3.xml");
        
controller4 :: XIAController4Port(RE AD4 CHID4, AD4, CHID4, 0.0.0.0, 2500, aa:aa:aa:aa:aa:aa);
router4 :: XIARouter2Port(RE AD4 RHID5, AD4, RHID5, 0.0.0.0, 2600, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
beacon4 :: XIASCIONBeaconServer(RE AD4 BHID4, AD4, BHID4, 0.0.0.0, 2700, aa:aa:aa:aa:aa:aa,
        AID 44444,
        CONFIG_FILE "./TD1/Non-TDC/AD4/beaconserver/conf/AD4BS.conf",
        TOPOLOGY_FILE "./TD1/Non-TDC/AD4/topology4.xml",
        ROT "./TD1/Non-TDC/AD4/beaconserver/ROT/rot-td1-0.xml");
path4 :: XIASCIONPathServer(RE AD4 PHID4, AD4, PHID4, 0.0.0.0, 3900, aa:aa:aa:aa:aa:aa,
        AID 44444,
        CONFIG_FILE "./TD1/Non-TDC/AD4/pathserver/conf/AD4PS.conf",
        TOPOLOGY_FILE "./TD1/Non-TDC/AD4/topology4.xml");

controller1[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]beacon1;
beacon1[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]controller1;

controller2[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]beacon2;
beacon2[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]controller2;

controller3[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]beacon3;
beacon3[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]controller3;

controller4[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]beacon4;
beacon4[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]controller4;


controller1[1] -> LinkUnqueue(0.005, 1 GB/s) -> [1]router1;
router1[1] -> LinkUnqueue(0.005, 1 GB/s) -> [1]controller1;

controller2[1] -> LinkUnqueue(0.005, 1 GB/s) -> [1]router21;
router21[1] -> LinkUnqueue(0.005, 1 GB/s) -> [1]controller2;

controller3[1] -> LinkUnqueue(0.005, 1 GB/s) -> [1]router3;
router3[1] -> LinkUnqueue(0.005, 1 GB/s) -> [1]controller3;

controller4[1] -> LinkUnqueue(0.005, 1 GB/s) -> [1]router4;
router4[1] -> LinkUnqueue(0.005, 1 GB/s) -> [1]controller4;


controller1[2] -> LinkUnqueue(0.005, 1 GB/s) -> [0]path1;
path1[0] -> LinkUnqueue(0.005, 1 GB/s) -> [2]controller1;

controller2[2] -> LinkUnqueue(0.005, 1 GB/s) -> [0]path2;
path2[0] -> LinkUnqueue(0.005, 1 GB/s) -> [2]controller2;

controller3[2] -> LinkUnqueue(0.005, 1 GB/s) -> [0]path3;
path3[0] -> LinkUnqueue(0.005, 1 GB/s) -> [2]controller3;

controller4[2] -> LinkUnqueue(0.005, 1 GB/s) -> [0]path4;
path4[0] -> LinkUnqueue(0.005, 1 GB/s) -> [2]controller4;


router1[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router21;
router21[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router1;

router21[2] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router22;
router22[0] -> LinkUnqueue(0.005, 1 GB/s) -> [2]router21;

router22[1] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router3;
router3[0] -> LinkUnqueue(0.005, 1 GB/s) -> [1]router22;

router22[2] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router4;
router4[0] -> LinkUnqueue(0.005, 1 GB/s) -> [2]router22;


Idle -> [3]router21;
router21[3] -> Idle;

Idle -> [3]router22;
router22[3] -> Idle;

Idle -> [3]controller1[3] -> Idle;
Idle -> [3]controller2[3] -> Idle;
Idle -> [3]controller3[3] -> Idle;
Idle -> [3]controller4[3] -> Idle;


// The following line is required by the xianet script so it can determine the appropriate
// host/router pair to run the nameserver on
// controller0 :: nameserver

ControlSocket(tcp, 7777);

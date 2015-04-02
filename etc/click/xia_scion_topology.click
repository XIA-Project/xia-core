require(library xia_router_lib.click);
require(library xia_address.click);

controller1 :: XIAController4Port(RE AD1 CHID1, AD1, CHID1, 0.0.0.0, 1500, aa:aa:aa:aa:aa:aa);
router1 :: XIARouter2Port(RE AD1 RHID1, AD1, RHID1, 0.0.0.0, 1600, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
beacon1 :: XIONBeaconServerCore(RE AD1 BSHID1, AD1, BSHID1, 0.0.0.0, 1700, aa:aa:aa:aa:aa:aa,
        CONFIG_FILE "./TD1/TDC/AD1/beaconserver/conf/AD1BS.conf",
        TOPOLOGY_FILE "./TD1/TDC/AD1/topology1.xml");
path1 :: XIONPathServerCore(RE AD1 PSHID1, AD1, PSHID1, 0.0.0.0, 1800, aa:aa:aa:aa:aa:aa,
        CONFIG_FILE "./TD1/TDC/AD1/pathserver/conf/AD1PS.conf",
        TOPOLOGY_FILE "./TD1/TDC/AD1/topology1.xml");
cert1 :: XIONCertServerCore(RE AD1 CSHID1, AD1, CSHID1, 0.0.0.0, 1900, aa:aa:aa:aa:aa:aa,
        CONFIG_FILE "./TD1/TDC/AD1/certserver/conf/AD1CS.conf",
        TOPOLOGY_FILE "./TD1/TDC/AD1/topology1.xml");

controller2 :: XIAController4Port(RE AD2 CHID2, AD2, CHID2, 0.0.0.0, 2000, aa:aa:aa:aa:aa:aa);
router21 :: XIARouter4Port(RE AD2 RHID2, AD2, RHID2, 0.0.0.0, 2100,
        aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
router22 :: XIARouter4Port(RE AD2 RHID4, AD2, RHID4, 0.0.0.0, 2200, 
		aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
beacon2 :: XIONBeaconServer(RE AD2 BSHID2, AD2, BSHID2, 0.0.0.0, 2300, aa:aa:aa:aa:aa:aa,
        CONFIG_FILE "./TD1/Non-TDC/AD2/beaconserver/conf/AD2BS.conf",
        TOPOLOGY_FILE "./TD1/Non-TDC/AD2/topology2.xml");
path2 :: XIONPathServer(RE AD2 PSHID2, AD2, PSHID2, 0.0.0.0, 2400, aa:aa:aa:aa:aa:aa,
        CONFIG_FILE "./TD1/Non-TDC/AD2/pathserver/conf/AD2PS.conf",
        TOPOLOGY_FILE "./TD1/Non-TDC/AD2/topology2.xml");     
//cert2 :: XIONCertServer(RE AD2 CSHID2, AD2, CSHID2, 0.0.0.0, 2500, aa:aa:aa:aa:aa:aa,
//        CONFIG_FILE "./TD1/Non-TDC/AD2/certserver/conf/AD2CS.conf",
//        TOPOLOGY_FILE "./TD1/Non-TDC/AD2/topology2.xml");

controller3 :: XIAController4Port(RE AD3 CHID3, AD3, CHID3, 0.0.0.0, 2600, aa:aa:aa:aa:aa:aa);
router3 :: XIARouter4Port(RE AD3 RHID3, AD3, RHID3, 0.0.0.0, 2700,
        aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
beacon3 :: XIONBeaconServer(RE AD3 BSHID3, AD3, BSHID3, 0.0.0.0, 2800, aa:aa:aa:aa:aa:aa,
        CONFIG_FILE "./TD1/Non-TDC/AD3/beaconserver/conf/AD3BS.conf",
        TOPOLOGY_FILE "./TD1/Non-TDC/AD3/topology3.xml");
path3 :: XIONPathServer(RE AD3 PSHID3, AD3, PSHID3, 0.0.0.0, 2900, aa:aa:aa:aa:aa:aa,
        CONFIG_FILE "./TD1/Non-TDC/AD3/pathserver/conf/AD3PS.conf",
        TOPOLOGY_FILE "./TD1/Non-TDC/AD3/topology3.xml");
//cert3 :: XIONCertServer(RE AD3 CSHID3, AD3, CSHID3, 0.0.0.0, 3000, aa:aa:aa:aa:aa:aa,
//        CONFIG_FILE "./TD1/Non-TDC/AD3/certserver/conf/AD3CS.conf",
//        TOPOLOGY_FILE "./TD1/Non-TDC/AD3/topology3.xml");
         
controller4 :: XIAController4Port(RE AD4 CHID4, AD4, CHID4, 0.0.0.0, 3100, aa:aa:aa:aa:aa:aa);
router4 :: XIARouter4Port(RE AD4 RHID5, AD4, RHID5, 0.0.0.0, 3200,
        aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa);
beacon4 :: XIONBeaconServer(RE AD4 BSHID4, AD4, BSHID4, 0.0.0.0, 3300, aa:aa:aa:aa:aa:aa,
        CONFIG_FILE "./TD1/Non-TDC/AD4/beaconserver/conf/AD4BS.conf",
        TOPOLOGY_FILE "./TD1/Non-TDC/AD4/topology4.xml");
path4 :: XIONPathServer(RE AD4 PSHID4, AD4, PSHID4, 0.0.0.0, 3400, aa:aa:aa:aa:aa:aa,
        CONFIG_FILE "./TD1/Non-TDC/AD4/pathserver/conf/AD4PS.conf",
        TOPOLOGY_FILE "./TD1/Non-TDC/AD4/topology4.xml");
//cert4 :: XIONCertServer(RE AD4 CSHID4, AD4, CSHID4, 0.0.0.0, 3500, aa:aa:aa:aa:aa:aa,
//        CONFIG_FILE "./TD1/Non-TDC/AD4/certserver/conf/AD4CS.conf",
//        TOPOLOGY_FILE "./TD1/Non-TDC/AD4/topology4.xml");

enc4 :: XIONEncap(RE AD4 CHID5, AD4, CHID5, 0.0.0.0, 4000, aa:aa:aa:aa:aa:aa);

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


Idle -> [2]router3;
router3[2] -> Idle;

Idle -> [3]router3;
router3[3] -> Idle;

enc4[0] -> LinkUnqueue(0.005, 1 GB/s) -> [2]router4;
router4[2] -> LinkUnqueue(0.005, 1 GB/s) -> [0]enc4;

Idle -> [3]router4;
router4[3] -> Idle;

// controller <-> cert server
controller1[3] -> LinkUnqueue(0.005, 1 GB/s) -> [0]cert1;
cert1[0] -> LinkUnqueue(0.005, 1 GB/s) -> [3]controller1;

//controller2[3] -> LinkUnqueue(0.005, 1 GB/s) -> [0]cert2;
//cert2[0] -> LinkUnqueue(0.005, 1 GB/s) -> [3]controller2;

//controller3[3] -> LinkUnqueue(0.005, 1 GB/s) -> [0]cert3;
//cert3[0] -> LinkUnqueue(0.005, 1 GB/s) -> [3]controller3;

//controller4[3] -> LinkUnqueue(0.005, 1 GB/s) -> [0]cert4;
//cert4[0] -> LinkUnqueue(0.005, 1 GB/s) -> [3]controller4;

Idle -> [3]controller2[3] -> Idle;
Idle -> [3]controller3[3] -> Idle;
Idle -> [3]controller4[3] -> Idle;


// The following line is required by the xianet script so it can determine the appropriate
// host/router pair to run the nameserver on
// controller0 :: nameserver

ControlSocket(tcp, 7777);

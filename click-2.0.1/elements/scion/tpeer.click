require(library scion.clib)

////////////////////////////////////////////////////////////////////
//SCION Certificate Servers
///////////////////////////////////////////////////////////////////

// Tenma: add PrivateKey and Cert to click file
// It should be merged to config file and change config object
// config should be customized for each servers
// Now config file is for per AD, not per servers inside the AD

certserver1::SCIONCertServerCore(AID 33333,
TOPOLOGY_FILE "./TD1/TDC/AD1/topology1.xml",
CONFIG_FILE "./TD1/TDC/AD1/certserver/conf/AD1CS.conf", 
ROT "./TD1/TDC/AD1/certserver/ROT/rot-td1-1.xml",
PrivateKey "./TD1/TDC/AD1/certserver/privatekey/td1-ad1-0.key",
Cert "./TD1/TDC/AD1/certserver/certificates/td1-ad1-0.crt");

certserver2::SCIONCertServer(AID 33333, 
TOPOLOGY_FILE "./TD1/Non-TDC/AD2/topology2.xml",
CONFIG_FILE "./TD1/Non-TDC/AD2/certserver/conf/AD2CS.conf", 
ROT "./TD1/Non-TDC/AD2/certserver/ROT/rot-td1-0.xml",
PrivateKey "./TD1/Non-TDC/AD2/certserver/privatekey/td1-ad2-0.key",
Cert "./TD1/Non-TDC/AD2/certserver/certificates/td1-ad2-0.crt");

certserver3::SCIONCertServer(AID 33333, 
TOPOLOGY_FILE "./TD1/Non-TDC/AD3/topology3.xml",
CONFIG_FILE "./TD1/Non-TDC/AD3/certserver/conf/AD3CS.conf", 
ROT "./TD1/Non-TDC/AD3/certserver/ROT/rot-td1-0.xml",
PrivateKey "./TD1/Non-TDC/AD2/certserver/privatekey/td1-ad3-0.key",
Cert "./TD1/Non-TDC/AD3/certserver/certificates/td1-ad3-0.crt");

certserver4::SCIONCertServer(AID 33333, 
TOPOLOGY_FILE "./TD1/Non-TDC/AD4/topology4.xml",
CONFIG_FILE "./TD1/Non-TDC/AD4/certserver/conf/AD4CS.conf", 
ROT "./TD1/Non-TDC/AD4/certserver/ROT/rot-td1-0.xml",
PrivateKey "./TD1/Non-TDC/AD4/certserver/privatekey/td1-ad4-0.key",
Cert "./TD1/Non-TDC/AD4/certserver/certificates/td1-ad4-0.crt");

certserver5::SCIONCertServer(AID 33333, 
TOPOLOGY_FILE "./TD1/Non-TDC/AD5/topology5.xml",
CONFIG_FILE "./TD1/Non-TDC/AD5/certserver/conf/AD5CS.conf", 
ROT "./TD1/Non-TDC/AD5/certserver/ROT/rot-td1-0.xml",
PrivateKey "./TD1/Non-TDC/AD5/certserver/privatekey/td1-ad5-0.key",
Cert "./TD1/Non-TDC/AD5/certserver/certificates/td1-ad5-0.crt");

certserver6::SCIONCertServer(AID 33333, 
TOPOLOGY_FILE "./TD1/Non-TDC/AD6/topology6.xml",
CONFIG_FILE "./TD1/Non-TDC/AD6/certserver/conf/AD6CS.conf", 
ROT "./TD1/Non-TDC/AD6/certserver/ROT/rot-td1-0.xml",
PrivateKey "./TD1/Non-TDC/AD6/certserver/privatekey/td1-ad6-0.key",
Cert "./TD1/Non-TDC/AD6/certserver/certificates/td1-ad6-0.crt");

certserver7::SCIONCertServer(AID 33333, 
TOPOLOGY_FILE "./TD1/Non-TDC/AD7/topology7.xml",
CONFIG_FILE "./TD1/Non-TDC/AD7/certserver/conf/AD7CS.conf", 
ROT "./TD1/Non-TDC/AD7/certserver/ROT/rot-td1-0.xml",
PrivateKey "./TD1/Non-TDC/AD7/certserver/privatekey/td1-ad7-0.key",
Cert "./TD1/Non-TDC/AD7/certserver/certificates/td1-ad7-0.crt");

////////////////////////////////////////////////////////////////////
//SCION Beacon Servers
///////////////////////////////////////////////////////////////////

pcbmaker::SCIONBeaconServerCore(AID 11111, 
CONFIG_FILE "./TD1/TDC/AD1/beaconserver/conf/AD1BS.conf",
TOPOLOGY_FILE ."/TD1/TDC/AD1/topology1.xml",
ROT "./TD1/TDC/AD1/beaconserver/ROT/rot-td1-1.xml");
// Tenma, use ROT as tempral parameter here, should put in config file. 

// Tenma, use ROT as tempral parameter here, should put in config file. 
pcb2::SCIONBeaconServer(AID 11111, 
CONFIG_FILE "./TD1/Non-TDC/AD2/beaconserver/conf/AD2BS.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD2/topology2.xml",
ROT "./TD1/Non-TDC/AD2/beaconserver/ROT/rot-td1-0.xml");

pcb3::SCIONBeaconServer(AID 11111, 
CONFIG_FILE "./TD1/Non-TDC/AD3/beaconserver/conf/AD3BS.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD3/topology3.xml",
ROT "./TD1/Non-TDC/AD3/beaconserver/ROT/rot-td1-0.xml");

pcb4::SCIONBeaconServer(AID 11111, 
CONFIG_FILE "./TD1/Non-TDC/AD4/beaconserver/conf/AD4BS.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD4/topology4.xml",
ROT "./TD1/Non-TDC/AD4/beaconserver/ROT/rot-td1-0.xml");

pcb5::SCIONBeaconServer(AID 11111, 
CONFIG_FILE "./TD1/Non-TDC/AD5/beaconserver/conf/AD5BS.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD5/topology5.xml",
ROT "./TD1/Non-TDC/AD5/beaconserver/ROT/rot-td1-0.xml");

pcb6::SCIONBeaconServer(AID 11111, 
CONFIG_FILE "./TD1/Non-TDC/AD6/beaconserver/conf/AD6BS.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD6/topology6.xml",
ROT "./TD1/Non-TDC/AD6/beaconserver/ROT/rot-td1-0.xml");

pcb7::SCIONBeaconServer(AID 11111, 
CONFIG_FILE "./TD1/Non-TDC/AD7/beaconserver/conf/AD7BS.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD7/topology7.xml",
ROT "./TD1/Non-TDC/AD7/beaconserver/ROT/rot-td1-0.xml");


////////////////////////////////////////////////////////////////////
//SCION Path Servers
///////////////////////////////////////////////////////////////////

pathcore::SCIONPathServerCore(AID 22222,
CONFIG "./TD1/TDC/AD1/pathserver/conf/AD1PS.conf",
TOPOLOGY "./TD1/TDC/AD1/topology1.xml");
pathlocal5::SCIONPathServer(AID 22222, 
CONFIG "./TD1/Non-TDC/AD5/pathserver/conf/AD5PS.conf",
TOPOLOGY "./TD1/Non-TDC/AD5/topology5.xml");
pathlocal6::SCIONPathServer(AID 22222, 
CONFIG "./TD1/Non-TDC/AD6/pathserver/conf/AD6PS.conf",
TOPOLOGY "./TD1/Non-TDC/AD6/topology6.xml");
pathlocal7::SCIONPathServer(AID 22222, 
CONFIG "./TD1/Non-TDC/AD7/pathserver/conf/AD7PS.conf",
TOPOLOGY "./TD1/Non-TDC/AD7/topology7.xml");

client5::SCIONClient(ADAID 5, TARGET 7, PATHSERVER 22222, AID 111,
TOPOLOGY_FILE "./TD1/Non-TDC/AD5/topology5.xml")

client7::SCIONClient(ADAID 7, TARGET 5, PATHSERVER 22222, AID 111,
TOPOLOGY_FILE "./TD1/Non-TDC/AD7/topology7.xml")


////////////////////////////////////////////////////////////////////
//SCION Gateways
///////////////////////////////////////////////////////////////////

//Gateway for AD5
gw5::SCIONGatewayModule (222, "./TD1/Non-TDC/AD5/gateway/conf/AD5GW.conf", 
"./TD1/Non-TDC/AD5/topology5.xml", "./TD1/Non-TDC/AD5/gateway/conf/addrtable.conf");

//Gateway for AD7
gw6::SCIONGatewayModule (222, "./TD1/Non-TDC/AD6/gateway/conf/AD6GW.conf", 
"./TD1/Non-TDC/AD6/topology6.xml", "./TD1/Non-TDC/AD6/gateway/conf/addrtable.conf");

//Gateway for AD7
gw7::SCIONGatewayModule (222, "./TD1/Non-TDC/AD7/gateway/conf/AD7GW.conf", 
"./TD1/Non-TDC/AD7/topology7.xml", "./TD1/Non-TDC/AD7/gateway/conf/addrtable.conf");

////////////////////////////////////////////////////////////////////
//SCION Switches
///////////////////////////////////////////////////////////////////

switch1::SCIONSwitch(CONFIG_FILE
"./TD1/TDC/AD1/AD1.conf", TOPOLOGY_FILE
"./TD1/TDC/AD1/topology1.xml")

switch2::SCIONSwitch(CONFIG_FILE
"./TD1/Non-TDC/AD2/AD2.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD2/topology2.xml")

switch3::SCIONSwitch(CONFIG_FILE
"./TD1/Non-TDC/AD3/AD3.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD3/topology3.xml")

switch4::SCIONSwitch(CONFIG_FILE
"./TD1/Non-TDC/AD4/AD4.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD4/topology4.xml")

switch5::SCIONSwitch(CONFIG_FILE
"./TD1/Non-TDC/AD5/AD5.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD5/topology5.xml")

switch6::SCIONSwitch(CONFIG_FILE
"./TD1/Non-TDC/AD6/AD6.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD6/topology6.xml")

switch7::SCIONSwitch(CONFIG_FILE
"./TD1/Non-TDC/AD7/AD7.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD7/topology7.xml")


////////////////////////////////////////////////////////////////////
//SCION Routers
///////////////////////////////////////////////////////////////////

router11::SCIONRouter(AID 1111, 
CONFIG_FILE "./TD1/TDC/AD1/routers/conf/AD1R1.conf", 
TOPOLOGY_FILE "./TD1/TDC/AD1/topology1.xml")


router21::SCIONRouter(AID 1111, 
CONFIG_FILE "./TD1/Non-TDC/AD2/routers/conf/AD2R1.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD2/topology2.xml")

router22::SCIONRouter(AID 2222, 
CONFIG_FILE "./TD1/Non-TDC/AD2/routers/conf/AD2R2.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD2/topology2.xml")

router23::SCIONRouter(AID 3333, 
CONFIG_FILE "./TD1/Non-TDC/AD2/routers/conf/AD2R3.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD2/topology2.xml")

router31::SCIONRouter(AID 1111, 
CONFIG_FILE "./TD1/Non-TDC/AD3/routers/conf/AD3R1.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD3/topology3.xml")

router32::SCIONRouter(AID 2222, 
CONFIG_FILE "./TD1/Non-TDC/AD3/routers/conf/AD3R2.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD3/topology3.xml")

router33::SCIONRouter(AID 3333, 
CONFIG_FILE "./TD1/Non-TDC/AD3/routers/conf/AD3R3.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD3/topology3.xml")

router34::SCIONRouter(AID 4444, 
CONFIG_FILE "./TD1/Non-TDC/AD3/routers/conf/AD3R4.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD3/topology3.xml")

router41::SCIONRouter(AID 1111, 
CONFIG_FILE "./TD1/Non-TDC/AD4/routers/conf/AD4R1.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD4/topology4.xml")

router42::SCIONRouter(AID 2222, 
CONFIG_FILE "./TD1/Non-TDC/AD4/routers/conf/AD4R2.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD4/topology4.xml")

router43::SCIONRouter(AID 3333, 
CONFIG_FILE "./TD1/Non-TDC/AD4/routers/conf/AD4R3.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD4/topology4.xml")

router44::SCIONRouter(AID 4444, 
CONFIG_FILE "./TD1/Non-TDC/AD4/routers/conf/AD4R4.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD4/topology4.xml")

router51::SCIONRouter(AID 1111, 
CONFIG_FILE "./TD1/Non-TDC/AD5/routers/conf/AD5R1.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD5/topology5.xml")

router52::SCIONRouter(AID 2222, 
CONFIG_FILE "./TD1/Non-TDC/AD5/routers/conf/AD5R2.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD5/topology5.xml")

router61::SCIONRouter(AID 1111, 
CONFIG_FILE "./TD1/Non-TDC/AD6/routers/conf/AD6R1.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD6/topology6.xml")

router71::SCIONRouter(AID 1111, 
CONFIG_FILE "./TD1/Non-TDC/AD7/routers/conf/AD7R1.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD7/topology7.xml")

////////////////////////////////////////////////////////////////////
//Element Connection
///////////////////////////////////////////////////////////////////

//AD1 elements
pcbmaker->[0]switch1;
switch1[0]->Queue(1000)->[0]pcbmaker;

router11[0]->[1]switch1;
switch1[1]->[0]router11;

pathcore[0]->[2]switch1;
switch1[2]->Queue->[0]pathcore;

certserver1[0]->[3]switch1;
switch1[3]->Queue(1000)->[0]certserver1;


router11[1]->[1]router21;
router21[1]->[1]router11;

router11[2]->[2]router21;
router21[2]->[2]router11;

router11[3]->[3]router21;
router21[3]->[3]router11;

//AD2 elements
router21[0]->[0]switch2;
switch2[0]->[0]router21;

pcb2[0]->[1]switch2;
switch2[1]->Queue(1000)->[0]pcb2;

router22[0]->[2]switch2;
switch2[2]->[0]router22;

router23[0]->[3]switch2;
switch2[3]->[0]router23;

certserver2[0]->[4]switch2;
switch2[4]->Queue(1000)->[0]certserver2;


router22[1]->[1]router31;
router31[1]->[1]router22;
router22[2]->[2]router31;
router31[2]->[2]router22;
router22[3]->[3]router31;
router31[3]->[3]router22;

router23[1]->[1]router41;
router41[1]->[1]router23;
router23[2]->[2]router41;
router41[2]->[2]router23;
router23[3]->[3]router41;
router41[3]->[3]router23;

//AD3 elements
router31[0]->[0]switch3;
switch3[0]->[0]router31;

router32[0]->[1]switch3;
switch3[1]->[0]router32;

switch3[2]->Queue(1000)->[0]pcb3;
pcb3[0]->[2]switch3;

switch3[3]->[0]router33;
router33[0]->[3]switch3;

switch3[4]->[0]router34;
router34[0]->[4]switch3;

certserver3[0]->[5]switch3;
switch3[5]->Queue(1000)->[0]certserver3;

router34[1]->[1]router44;
router44[1]->[1]router34;
router34[2]->[2]router44;
router44[2]->[2]router34;
router34[3]->[3]router44;
router44[3]->[3]router34;

router33[1]->[1]router61;
router61[1]->[1]router33;
router33[2]->[2]router61;
router61[2]->[2]router33;
router33[3]->[3]router61;
router61[3]->[3]router33;

//AD6 elements
router61[0]->[0]switch6;
switch6[0]->[0]router61;

pcb6[0]->[1]switch6;
switch6[1]->Queue(1000)->[0]pcb6;

pathlocal6[0]->[2]switch6;
switch6[2]->Queue->[0]pathlocal6;

certserver6[0]->[3]switch6;
switch6[3]->Queue(1000)->[0]certserver6;

//AD4 elements
router41[0]->[0]switch4;
switch4[0]->[0]router41;

router42[0]->[1]switch4;
switch4[1]->[0]router42;

pcb4[0]->[2]switch4;
switch4[2]->Queue(1000)->[0]pcb4;

router43[0]->[3]switch4;
switch4[3]->[0]router43;

switch4[4]->[0]router44;
router44[0]->[4]switch4;

certserver4[0]->[5]switch4;
switch4[5]->Queue(1000)->[0]certserver4;

router43[1]->[1]router71;
router71[1]->[1]router43;
router43[2]->[2]router71;
router71[2]->[2]router43;
router43[3]->[3]router71;
router71[3]->[3]router43;

router71[0]->[0]switch7;
switch7[0]->[0]router71;

pcb7[0]->[1]switch7;
switch7[1]->Queue(1000)->[0]pcb7;

pathlocal7[0]->[2]switch7;
switch7[2]->Queue->[0]pathlocal7;

certserver7[0]->[3]switch7;
switch7[3]->Queue(1000)->[0]certserver7;


router42[1]->[1]router51;
router51[1]->[1]router42;
router42[2]->[2]router51;
router51[2]->[2]router42;
router42[3]->[3]router51;
router51[3]->[3]router42;

router32[1]->[1]router52;
router52[1]->[1]router32;
router32[2]->[2]router52;
router52[2]->[2]router32;
router32[3]->[3]router52;
router52[3]->[3]router32;

//AD5 elements
router51[0]->[0]switch5;
switch5[0]->[0]router51;

router52[0]->[1]switch5;
switch5[1]->[0]router52;

pcb5[0]->[2]switch5;
switch5[2]->Queue(1000)->[0]pcb5;

switch5[3]->Queue->[0]pathlocal5;
pathlocal5[0]->[3]switch5;

certserver5[0]->[4]switch5;
switch5[4]->Queue(1000)->[0]certserver5;

//Connect Certificate Servers

//Connect Clients to AD5 and AD7
client5[0]->[6]switch5;
switch5[6]->Queue->[0]client5;

client7[0]->[5]switch7;
switch7[5]->Queue->[0]client7;

//Connect Gateway elements for AD5
gw5[0]->[5]switch5;
switch5[5]->[0]gw5;

TimedSource(INTERVAL 1.001)->IPEncap(4, 192.168.0.5, 192.168.0.7)->Print("Gateway5 Send")->[1]gw5;
gw5[1]->Print("Gateway5 Received")->Discard;

//Connect Gateway elements for AD6
gw6[0]->[4]switch6;
switch6[4]->[0]gw6;

TimedSource(INTERVAL 1.001,DATA \<00 00 00 00 11 11 11 11
//InfiniteSource(DATA \<00 00 00 00 11 11 11 11
11 11 11 11 11 11 11 11 01 02 03 04 05 06 07 08
11 11 11 11 11 11 11 11 01 02 03 04 05 06 07 08
11 11 11 11 11 11 11 11 01 02 03 04 05 06 07 08
11 11 11 11 11 11 11 11 01 02 03 04 05 06 07 08
11 11 11 11 11 11 11 11 01 02 03 04 05 06 07 08
11 11 11 11 11 11 11 11 01 02 03 04 05 06 07 08
11 11 11 11 11 11 11 11 01 02 03 04 05 06 07 08
11 11 11 11 11 11 11 11 01 02 03 04 05 06 07 08
11 11 11 11 11 11 11 11 01 02 03 04 05 06 07 08
11 11 11 11 11 11 11 11 01 02 03 04 05 06 07 08
11 11 11 11 11 11 11 11 01 02 03 04 05 06 07 08
//>, LENGTH 1500, BURST 100)->IPEncap(4, 192.168.0.6, 192.168.0.7)->Print("Gateway6 Send")->[1]gw6;
>)->IPEncap(4, 192.168.0.6, 192.168.0.7)->Print("Gateway6 Send")->[1]gw6;

gw6[1]->Print("Gateway6 Received")->Discard;


//Connect Gateway elements for AD7
gw7[0]->[4]switch7;
switch7[4]->[0]gw7;

//Idle->[0]encap7;
TimedSource(INTERVAL 1.001,DATA \<00 00 00 00 11 11 11 11
//InfiniteSource(DATA1\<00 00 00 00 11 11 11 11
11 11 11 11 11 11 11 11 01 02 03 04 05 06 07 08
11 11 11 11 11 11 11 11 01 02 03 04 05 06 07 08
11 11 11 11 11 11 11 11 01 02 03 04 05 06 07 08
11 11 11 11 11 11 11 11 01 02 03 04 05 06 07 08
11 11 11 11 11 11 11 11 01 02 03 04 05 06 07 08
11 11 11 11 11 11 11 11 01 02 03 04 05 06 07 08
11 11 11 11 11 11 11 11 01 02 03 04 05 06 07 08
11 11 11 11 11 11 11 11 01 02 03 04 05 06 07 08
11 11 11 11 11 11 11 11 01 02 03 04 05 06 07 08
11 11 11 11 11 11 11 11 01 02 03 04 05 06 07 08
11 11 11 11 11 11 11 11 01 02 03 04 05 06 07 08
//>, LENGTH 1500, BURST 100)->IPEncap(4, 192.168.0.7, 192.168.0.6)->Print("Gateway7 Send")->[1]gw7;
>)->IPEncap(4, 192.168.0.7, 192.168.0.6)->Print("Gateway7 Send")->[1]gw7;

gw7[1]->Print("Gateway7 Received")->Discard;

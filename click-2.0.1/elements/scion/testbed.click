require(library scion.clib)

////////////////////////////////////////////////////////////////////
//SCION Certificate Servers
///////////////////////////////////////////////////////////////////

certserver1::SCIONCertServerCore(AID 33333,
TOPOLOGY_FILE "./TD1/TDC/AD1/topology1.xml",
CONFIG_FILE "./TD1/TDC/AD1/certserver/conf/AD1CS.conf", 
ROT "./TD1/TDC/AD1/certserver/ROT/rot-td1-0.xml",
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

beaconserver1::SCIONBeaconServerCore(AID 11111, 
CONFIG_FILE "./TD1/TDC/AD1/beaconserver/conf/AD1BS.conf",
TOPOLOGY_FILE ."/TD1/TDC/AD1/topology1.xml",
ROT "./TD1/TDC/AD1/beaconserver/ROT/rot-td1-0.xml");

beaconserver2::SCIONBeaconServer(AID 11111, 
CONFIG_FILE "./TD1/Non-TDC/AD2/beaconserver/conf/AD2BS.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD2/topology2.xml",
ROT "./TD1/Non-TDC/AD2/beaconserver/ROT/rot-td1-0.xml");

beaconserver3::SCIONBeaconServer(AID 11111, 
CONFIG_FILE "./TD1/Non-TDC/AD3/beaconserver/conf/AD3BS.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD3/topology3.xml",
ROT "./TD1/Non-TDC/AD3/beaconserver/ROT/rot-td1-0.xml");

beaconserver4::SCIONBeaconServer(AID 11111, 
CONFIG_FILE "./TD1/Non-TDC/AD4/beaconserver/conf/AD4BS.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD4/topology4.xml",
ROT "./TD1/Non-TDC/AD4/beaconserver/ROT/rot-td1-0.xml");

beaconserver5::SCIONBeaconServer(AID 11111, 
CONFIG_FILE "./TD1/Non-TDC/AD5/beaconserver/conf/AD5BS.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD5/topology5.xml",
ROT "./TD1/Non-TDC/AD5/beaconserver/ROT/rot-td1-0.xml");

beaconserver6::SCIONBeaconServer(AID 11111, 
CONFIG_FILE "./TD1/Non-TDC/AD6/beaconserver/conf/AD6BS.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD6/topology6.xml",
ROT "./TD1/Non-TDC/AD6/beaconserver/ROT/rot-td1-0.xml");

beaconserver7::SCIONBeaconServer(AID 11111, 
CONFIG_FILE "./TD1/Non-TDC/AD7/beaconserver/conf/AD7BS.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD7/topology7.xml",
ROT "./TD1/Non-TDC/AD7/beaconserver/ROT/rot-td1-0.xml");


////////////////////////////////////////////////////////////////////
//SCION Path Servers
///////////////////////////////////////////////////////////////////

pathserver1::SCIONPathServerCore(AID 22222,
CONFIG "./TD1/TDC/AD1/pathserver/conf/AD1PS.conf",
TOPOLOGY "./TD1/TDC/AD1/topology1.xml");
pathserver5::SCIONPathServer(AID 22222, 
CONFIG "./TD1/Non-TDC/AD5/pathserver/conf/AD5PS.conf",
TOPOLOGY "./TD1/Non-TDC/AD5/topology5.xml");
pathserver6::SCIONPathServer(AID 22222, 
CONFIG "./TD1/Non-TDC/AD6/pathserver/conf/AD6PS.conf",
TOPOLOGY "./TD1/Non-TDC/AD6/topology6.xml");
pathserver7::SCIONPathServer(AID 22222, 
CONFIG "./TD1/Non-TDC/AD7/pathserver/conf/AD7PS.conf",
TOPOLOGY "./TD1/Non-TDC/AD7/topology7.xml");

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

////////////////////////////////////////////////////////
//AD1 Routers
////////////////////////////////////////////////////////
router12::SCIONRouter(AID 1111, 
CONFIG_FILE "./TD1/TDC/AD1/routers/conf/AD1R12.conf", 
TOPOLOGY_FILE "./TD1/TDC/AD1/topology1.xml")

router13::SCIONRouter(AID 2222, 
CONFIG_FILE "./TD1/TDC/AD1/routers/conf/AD1R13.conf", 
TOPOLOGY_FILE "./TD1/TDC/AD1/topology1.xml")

router14::SCIONRouter(AID 3333, 
CONFIG_FILE "./TD1/TDC/AD1/routers/conf/AD1R14.conf", 
TOPOLOGY_FILE "./TD1/TDC/AD1/topology1.xml")
////////////////////////////////////////////////////////

////////////////////////////////////////////////////////
//AD2 Routers
////////////////////////////////////////////////////////
router21::SCIONRouter(AID 1111, 
CONFIG_FILE "./TD1/Non-TDC/AD2/routers/conf/AD2R21.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD2/topology2.xml")

router23::SCIONRouter(AID 2222, 
CONFIG_FILE "./TD1/Non-TDC/AD2/routers/conf/AD2R23.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD2/topology2.xml")

router25::SCIONRouter(AID 3333, 
CONFIG_FILE "./TD1/Non-TDC/AD2/routers/conf/AD2R25.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD2/topology2.xml")

router26::SCIONRouter(AID 4444, 
CONFIG_FILE "./TD1/Non-TDC/AD2/routers/conf/AD2R26.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD2/topology2.xml")
////////////////////////////////////////////////////////

////////////////////////////////////////////////////////
//AD3 Routers
////////////////////////////////////////////////////////
router31::SCIONRouter(AID 1111, 
CONFIG_FILE "./TD1/Non-TDC/AD3/routers/conf/AD3R31.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD3/topology3.xml")

router32::SCIONRouter(AID 2222, 
CONFIG_FILE "./TD1/Non-TDC/AD3/routers/conf/AD3R32.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD3/topology3.xml")

router34::SCIONRouter(AID 3333, 
CONFIG_FILE "./TD1/Non-TDC/AD3/routers/conf/AD3R34.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD3/topology3.xml")

router36::SCIONRouter(AID 4444, 
CONFIG_FILE "./TD1/Non-TDC/AD3/routers/conf/AD3R36.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD3/topology3.xml")
////////////////////////////////////////////////////////

////////////////////////////////////////////////////////
//AD4 Routers
////////////////////////////////////////////////////////
router41::SCIONRouter(AID 1111, 
CONFIG_FILE "./TD1/Non-TDC/AD4/routers/conf/AD4R41.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD4/topology4.xml")

router43::SCIONRouter(AID 2222, 
CONFIG_FILE "./TD1/Non-TDC/AD4/routers/conf/AD4R43.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD4/topology4.xml")

router47::SCIONRouter(AID 3333, 
CONFIG_FILE "./TD1/Non-TDC/AD4/routers/conf/AD4R47.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD4/topology4.xml")
////////////////////////////////////////////////////////

////////////////////////////////////////////////////////
//AD5 Routers
////////////////////////////////////////////////////////
router52::SCIONRouter(AID 1111, 
CONFIG_FILE "./TD1/Non-TDC/AD5/routers/conf/AD5R52.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD5/topology5.xml")
////////////////////////////////////////////////////////

////////////////////////////////////////////////////////
//AD6 Routers
////////////////////////////////////////////////////////
router62::SCIONRouter(AID 1111, 
CONFIG_FILE "./TD1/Non-TDC/AD6/routers/conf/AD6R62.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD6/topology6.xml")

router63::SCIONRouter(AID 2222, 
CONFIG_FILE "./TD1/Non-TDC/AD6/routers/conf/AD6R63.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD6/topology6.xml")
////////////////////////////////////////////////////////

////////////////////////////////////////////////////////
//AD7 Routers
////////////////////////////////////////////////////////
router74::SCIONRouter(AID 1111, 
CONFIG_FILE "./TD1/Non-TDC/AD7/routers/conf/AD7R74.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD7/topology7.xml")

////////////////////////////////////////////////////////////////////
//Element Connection
///////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
//AD1 elements
///////////////////////////////////////////////////////////////////
beaconserver1[0]->[0]switch1;
switch1[0]->Queue(1000)->[0]beaconserver1;

certserver1[0]->[1]switch1;
switch1[1]->Queue(1000)->[0]certserver1;

pathserver1[0]->[2]switch1;
switch1[2]->Queue(1000)->[0]pathserver1;

//Intra-AD connection
router12[0]->[3]switch1;
switch1[3]->[0]router12;

router13[0]->[4]switch1;
switch1[4]->[0]router13;

router14[0]->[5]switch1;
switch1[5]->[0]router14;

//Inter-AD connection
router12[1]->[1]router21;
router21[1]->[1]router12;

router13[1]->[1]router31;
router31[1]->[1]router13;

router14[1]->[1]router41;
router41[1]->[1]router14;

///////////////////////////////////////////////////////////////////
//AD2 elements
///////////////////////////////////////////////////////////////////
beaconserver2[0]->[0]switch2;
switch2[0]->Queue(1000)->[0]beaconserver2;

certserver2[0]->[1]switch2;
switch2[1]->Queue(1000)->[0]certserver2;

//Intra-AD connection
router21[0]->[2]switch2;
switch2[2]->[0]router21;

router23[0]->[3]switch2;
switch2[3]->[0]router23;

router25[0]->[4]switch2;
switch2[4]->[0]router25;

router26[0]->[5]switch2;
switch2[5]->[0]router26;

//Inter-AD connection
router23[1]->[1]router32;
router32[1]->[1]router23;

router25[1]->[1]router52;
router52[1]->[1]router25;

router26[1]->[1]router62;
router62[1]->[1]router26;

///////////////////////////////////////////////////////////////////
//AD3 elements
///////////////////////////////////////////////////////////////////
beaconserver3[0]->[0]switch3;
switch3[0]->Queue(1000)->[0]beaconserver3;

certserver3[0]->[1]switch3;
switch3[1]->Queue(1000)->[0]certserver3;

//Intra-AD connection
router31[0]->[2]switch3;
switch3[2]->[0]router31;

router32[0]->[3]switch3;
switch3[3]->[0]router32;

router34[0]->[4]switch3;
switch3[4]->[0]router34;

router36[0]->[5]switch3;
switch3[5]->[0]router36;

//Inter-AD connection
router34[1]->[1]router43;
router43[1]->[1]router34;

router36[1]->[1]router63;
router63[1]->[1]router36;

///////////////////////////////////////////////////////////////////
//AD4 elements
///////////////////////////////////////////////////////////////////
beaconserver4[0]->[0]switch4;
switch4[0]->Queue(1000)->[0]beaconserver4;

certserver4[0]->[1]switch4;
switch4[1]->Queue(1000)->[0]certserver4;

//Intra-AD connection
router41[0]->[2]switch4;
switch4[2]->[0]router41;

router43[0]->[3]switch4;
switch4[3]->[0]router43;

router47[0]->[4]switch4;
switch4[4]->[0]router47;

//Inter-AD connection
router47[1]->[1]router74;
router74[1]->[1]router47;

///////////////////////////////////////////////////////////////////
//AD5 elements
///////////////////////////////////////////////////////////////////
beaconserver5[0]->[0]switch5;
switch5[0]->Queue(1000)->[0]beaconserver5;

certserver5[0]->[1]switch5;
switch5[1]->Queue(1000)->[0]certserver5;

pathserver5[0]->[2]switch5;
switch5[2]->Queue(1000)->[0]pathserver5;


//Intra-AD connection
router52[0]->[3]switch5;
switch5[3]->[0]router52;

//Connect Gateway elements
gw5[0]->[4]switch5;
switch5[4]->[0]gw5;

///////////////////////////////////////////////////////////////////
//AD6 elements
///////////////////////////////////////////////////////////////////
beaconserver6[0]->[0]switch6;
switch6[0]->Queue(1000)->[0]beaconserver6;

certserver6[0]->[1]switch6;
switch6[1]->Queue(1000)->[0]certserver6;

pathserver6[0]->[2]switch6;
switch6[2]->Queue(1000)->[0]pathserver6;

//Intra-AD connection
router62[0]->[3]switch6;
switch6[3]->[0]router62;

router63[0]->[4]switch6;
switch6[4]->[0]router63;

//Connect Gateway elements
gw6[0]->[5]switch6;
switch6[5]->[0]gw6;

///////////////////////////////////////////////////////////////////
//AD7 elements
///////////////////////////////////////////////////////////////////
beaconserver7[0]->[0]switch7;
switch7[0]->Queue(1000)->[0]beaconserver7;

certserver7[0]->[1]switch7;
switch7[1]->Queue(1000)->[0]certserver7;

pathserver7[0]->[2]switch7;
switch7[2]->Queue(1000)->[0]pathserver7;

//Intra-AD connection
router74[0]->[3]switch7;
switch7[3]->[0]router74;

//Connect Gateway elements
gw7[0]->[4]switch7;
switch7[4]->[0]gw7;

///////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
//Traffic Generation for test
///////////////////////////////////////////////////////////////////

TimedSource(INTERVAL 1.001)->IPEncap(4, 192.168.0.5, 192.168.0.7)->Print("Gateway5 Send")->[1]gw5;
gw5[1]->Print("Gateway5 Received")->Discard;

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

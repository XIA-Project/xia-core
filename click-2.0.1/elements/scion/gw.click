pathcore::SCIONPathServerCore(AID 22222, CONFIG "./TD1/TDC/AD1/AD1.conf",TOPOLOGY
"./TD1/TDC/AD1/topology1.xml");
pathlocal5::SCIONPathServer(AID 22222, CONFIG "./TD1/Non-TDC/AD5/AD5.conf",
TOPOLOGY "./TD1/Non-TDC/AD5/topology5.xml");
pathlocal6::SCIONPathServer(AID 22222, CONFIG "./TD1/Non-TDC/AD6/AD6.conf",
TOPOLOGY "./TD1/Non-TDC/AD6/topology6.xml");
pathlocal7::SCIONPathServer(AID 22222, CONFIG "./TD1/Non-TDC/AD7/AD7.conf",
TOPOLOGY "./TD1/Non-TDC/AD7/topology7.xml");

client5::SCIONClient(ADAID 5, TARGET 7, PATHSERVER 22222, AID 111,
TOPOLOGY_FILE "./TD1/Non-TDC/AD5/topology5.xml")

client7::SCIONClient(ADAID 7, TARGET 5, PATHSERVER 22222, AID 111,
TOPOLOGY_FILE "./TD1/Non-TDC/AD7/topology7.xml")


//Gateway element
//Syntax Encap <-> Gateway <-> Switch
//       Decap <-
pathinfo5::SCIONPathInfo(TIMEOUT 100);
pathinfo7::SCIONPathInfo(TIMEOUT 100);

//Gateway for AD5
encap5::SCIONEncap(AID 222, CONFIG_FILE "./TD1/Non-TDC/AD5/AD5.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD5/topology5.xml", 
ADDR_TABLE "./TD1/Non-TDC/AD5/addrtable.conf",
PATHINFO pathinfo5)
gateway5::SCIONGateway(AID 222)
decap5::SCIONDecap(PATHINFO pathinfo5)

//Gateway for AD7
encap7::SCIONEncap(AID 222, CONFIG_FILE "./TD1/Non-TDC/AD7/AD7.conf", 
TOPOLOGY_FILE "./TD1/Non-TDC/AD7/topology7.xml", 
ADDR_TABLE "./TD1/Non-TDC/AD7/addrtable.conf",
PATHINFO pathinfo7)
gateway7::SCIONGateway(AID 222)
decap7::SCIONDecap(PATHINFO pathinfo7)


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


pcbmaker::SCIONBeaconServerCore(AID 11111, CONFIG_FILE
"./TD1/TDC/AD1/AD1.conf",TOPOLOGY_FILE ./TD1/TDC/AD1/topology1.xml);


router11::SCIONRouter(AID 1111, CONFIG_FILE
"./TD1/TDC/AD1/AD1R1.conf", TOPOLOGY_FILE
"./TD1/TDC/AD1/topology1.xml")


router21::SCIONRouter(AID 1111, CONFIG_FILE
"./TD1/Non-TDC/AD2/AD2R1.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD2/topology2.xml")

router22::SCIONRouter(AID 2222, CONFIG_FILE
"./TD1/Non-TDC/AD2/AD2R2.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD2/topology2.xml")

router23::SCIONRouter(AID 3333, CONFIG_FILE
"./TD1/Non-TDC/AD2/AD2R3.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD2/topology2.xml")


router31::SCIONRouter(AID 1111, CONFIG_FILE
"./TD1/Non-TDC/AD3/AD3R1.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD3/topology3.xml")

router32::SCIONRouter(AID 2222, CONFIG_FILE
"./TD1/Non-TDC/AD3/AD3R2.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD3/topology3.xml")

router33::SCIONRouter(AID 3333, CONFIG_FILE
"./TD1/Non-TDC/AD3/AD3R3.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD3/topology3.xml")

router34::SCIONRouter(AID 4444, CONFIG_FILE
"./TD1/Non-TDC/AD3/AD3R4.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD3/topology3.xml")

router41::SCIONRouter(AID 1111, CONFIG_FILE
"./TD1/Non-TDC/AD4/AD4R1.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD4/topology4.xml")

router42::SCIONRouter(AID 2222, CONFIG_FILE
"./TD1/Non-TDC/AD4/AD4R2.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD4/topology4.xml")

router43::SCIONRouter(AID 3333, CONFIG_FILE
"./TD1/Non-TDC/AD4/AD4R3.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD4/topology4.xml")

router44::SCIONRouter(AID 4444, CONFIG_FILE
"./TD1/Non-TDC/AD4/AD4R4.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD4/topology4.xml")


router51::SCIONRouter(AID 1111, CONFIG_FILE
"./TD1/Non-TDC/AD5/AD5R1.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD5/topology5.xml")

router52::SCIONRouter(AID 2222, CONFIG_FILE
"./TD1/Non-TDC/AD5/AD5R2.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD5/topology5.xml")

router61::SCIONRouter(AID 1111, CONFIG_FILE
"./TD1/Non-TDC/AD6/AD6R1.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD6/topology6.xml")

router71::SCIONRouter(AID 1111, CONFIG_FILE
"./TD1/Non-TDC/AD7/AD7R1.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD7/topology7.xml")


pcb2::SCIONBeaconServer(AID 11111, CONFIG_FILE
            "./TD1/Non-TDC/AD2/AD2.conf", TOPOLOGY_FILE
            "./TD1/Non-TDC/AD2/topology2.xml");

pcb3::SCIONBeaconServer(AID 11111, CONFIG_FILE
            "./TD1/Non-TDC/AD3/AD3.conf", TOPOLOGY_FILE
            "./TD1/Non-TDC/AD3/topology3.xml");

pcb4::SCIONBeaconServer(AID 11111, CONFIG_FILE
            "./TD1/Non-TDC/AD4/AD4.conf", TOPOLOGY_FILE
            "./TD1/Non-TDC/AD4/topology4.xml");

pcb5::SCIONBeaconServer(AID 11111, CONFIG_FILE
            "./TD1/Non-TDC/AD5/AD5.conf", TOPOLOGY_FILE
            "./TD1/Non-TDC/AD5/topology5.xml");

pcb6::SCIONBeaconServer(AID 11111, CONFIG_FILE
            "./TD1/Non-TDC/AD6/AD6.conf", TOPOLOGY_FILE
            "./TD1/Non-TDC/AD6/topology6.xml");

pcb7::SCIONBeaconServer(AID 11111, CONFIG_FILE
            "./TD1/Non-TDC/AD7/AD7.conf", TOPOLOGY_FILE
            "./TD1/Non-TDC/AD7/topology7.xml");

pcbmaker->[0]switch1;
switch1[0]->Queue(1000)->[0]pcbmaker;

router11[0]->[1]switch1;
switch1[1]->[0]router11;

pathcore[0]->[2]switch1;
switch1[2]->Queue->[0]pathcore;

router11[1]->[1]router21;
router21[1]->[1]router11;

router11[2]->[2]router21;
router21[2]->[2]router11;

router11[3]->[3]router21;
router21[3]->[3]router11;


router21[0]->[0]switch2;
switch2[0]->[0]router21;

pcb2[0]->[1]switch2;
switch2[1]->Queue(1000)->[0]pcb2;

router22[0]->[2]switch2;
switch2[2]->[0]router22;

router23[0]->[3]switch2;
switch2[3]->[0]router23;


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

router61[0]->[0]switch6;
switch6[0]->[0]router61;

pcb6[0]->[1]switch6;
switch6[1]->Queue(1000)->[0]pcb6;

pathlocal6[0]->[2]switch6;
switch6[2]->Queue->[0]pathlocal6;


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

router51[0]->[0]switch5;
switch5[0]->[0]router51;


router52[0]->[1]switch5;
switch5[1]->[0]router52;

pcb5[0]->[2]switch5;
switch5[2]->Queue(1000)->[0]pcb5;


switch5[3]->Queue->[0]pathlocal5;
pathlocal5[0]->[3]switch5;

//Connect Clients to AD5 and AD7
client5[0]->[4]switch5;
switch5[4]->Queue->[0]client5;

client7[0]->[4]switch7;
switch7[4]->Queue->[0]client7;

//Connect Gateway elements for AD5
gateway5[0]->[5]switch5;
switch5[5]->[0]gateway5;

TimedSource(INTERVAL 0.1)->IPEncap(4, 192.168.0.2, 192.168.0.3)->Print("Gateway5 Send")->[0]encap5;
encap5[0]->[1]gateway5;
gateway5[1]->[1]encap5;

gateway5[2]->[0]decap5->Print("Gateway5 Received")->Discard;

//Connect Gateway elements for AD7
gateway7[0]->[3]switch7;
switch7[3]->[0]gateway7;

//Idle->[0]encap7;
TimedSource(INTERVAL 0.1)->IPEncap(4, 192.168.0.3, 192.168.0.2)->Print("Gateway7 Send")->[0]encap7;
encap7[0]->[1]gateway7;
gateway7[1]->[1]encap7;

gateway7[2]->[0]decap7->Print("Gateway7 Received")->Discard;

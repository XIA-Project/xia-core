certserver1::SCIONCertServerCore(AID 33333, TOPOLOGY_FILE ./TD1/TDC/AD1/topology1.xml,
CONFIG_FILE ./TD1/TDC/AD1/AD1.conf, ROT_FILE ./TD1/TDC/AD1/certserver/ROT/ROT.xml)
certserver2::SCIONCertServer(AID 33333, TOPOLOGY_FILE ./TD1/Non-TDC/AD2/topology2.xml,
CONFIG_FILE ./TD1/Non-TDC/AD2/AD2.conf, ROT_FILE ./TD1/Non-TDC/AD2/certserver/ROT/ROT.xml)
certserver3::SCIONCertServer(AID 33333, TOPOLOGY_FILE ./TD1/Non-TDC/AD3/topology3.xml,
CONFIG_FILE ./TD1/Non-TDC/AD3/AD3.conf, ROT_FILE ./TD1/Non-TDC/AD3/certserver/ROT/ROT.xml)
certserver4::SCIONCertServer(AID 33333, TOPOLOGY_FILE ./TD1/Non-TDC/AD4/topology4.xml,
CONFIG_FILE ./TD1/Non-TDC/AD4/AD4.conf, ROT_FILE ./TD1/Non-TDC/AD4/certserver/ROT/ROT.xml)
certserver5::SCIONCertServer(AID 33333, TOPOLOGY_FILE ./TD1/Non-TDC/AD5/topology5.xml,
CONFIG_FILE ./TD1/Non-TDC/AD5/AD5.conf, ROT_FILE ./TD1/Non-TDC/AD5/certserver/ROT/ROT.xml)
certserver6::SCIONCertServer(AID 33333, TOPOLOGY_FILE ./TD1/Non-TDC/AD6/topology6.xml,
CONFIG_FILE ./TD1/Non-TDC/AD6/AD6.conf, ROT_FILE ./TD1/Non-TDC/AD6/certserver/ROT/ROT.xml)
certserver7::SCIONCertServer(AID 33333, TOPOLOGY_FILE ./TD1/Non-TDC/AD7/topology7.xml,
CONFIG_FILE ./TD1/Non-TDC/AD7/AD7.conf, ROT_FILE ./TD1/Non-TDC/AD7/certserver/ROT/ROT.xml)



//client6::SCIONClient(ADAID 6, TARGET 5, PATHSERVER 22222, LOGFILE "./TD1/Non-TDC/AD6/AD6C1.log", LOGLEVEL 300, AID 111)

//client7::SCIONClient(ADAID 7, TARGET 6, PATHSERVER 22222, LOGFILE "./TD1/Non-TDC/AD7/AD7C1.log", LOGLEVEL 300, AID 111)

encap6::SCIONEncap(SRC 6, TABLE ./aidtable.conf)
gateway6::SCIONGateway(AID 111)
decap6::SCIONDecap

encap7::SCIONEncap(SRC 7, TABLE ./aidtable.conf)
gateway7::SCIONGateway(AID 111)
decap7::SCIONDecap

client5::SCIONClient(ADAID 5, TARGET 7, PATHSERVER 22222, LOGFILE
"./TD1/Non-TDC/AD5/AD5C1.log", LOGLEVEL 300, AID 111)


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

switch5::SCIONSwitch(CLIENT 4, CONFIG_FILE
"./TD1/Non-TDC/AD5/AD5.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD5/topology5.xml")

switch6::SCIONSwitch(CLIENT 3, CONFIG_FILE
"./TD1/Non-TDC/AD6/AD6.conf", TOPOLOGY_FILE
"./TD1/Non-TDC/AD6/topology6.xml")

switch7::SCIONSwitch(CLIENT 3, CONFIG_FILE
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

pathserver5::SCIONPathServer(AID 22222, CONFIG ./TD1/Non-TDC/AD5/AD5.conf, TOPOLOGY
./TD1/Non-TDC/AD5/topology5.xml);

pathserver6::SCIONPathServer(AID 22222, CONFIG ./TD1/Non-TDC/AD6/AD6.conf, TOPOLOGY
./TD1/Non-TDC/AD6/topology6.xml);

pathserver7::SCIONPathServer(AID 22222, CONFIG ./TD1/Non-TDC/AD7/AD7.conf, TOPOLOGY
./TD1/Non-TDC/AD7/topology7.xml);

pathservercore::SCIONPathServerCore(AID 22222, CONFIG ./TD1/TDC/AD1/AD1.conf);



pathservercore[0]->[2]switch1;
switch1[2]->Queue(1000)->[0]pathservercore;

pcbmaker->[0]switch1;
switch1[0]->Queue(1000)->[0]pcbmaker;

certserver1[0]->[3]switch1;
switch1[3]->Queue(1000)->[0]certserver1;

router11[0]->[1]switch1;
switch1[1]->[0]router11;

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

router61[0]->[0]switch6;
switch6[0]->[0]router61;

pcb6[0]->[1]switch6;
switch6[1]->Queue(1000)->[0]pcb6;

pathserver6[0]->[2]switch6;
switch6[2]->Queue(1000)->[0]pathserver6;

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

pathserver7[0]->[2]switch7;
switch7[2]->Queue(1000)->[0]pathserver7;

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

switch5[3]->Queue(1000)->[0]pathserver5;
pathserver5[0]->[3]switch5;

switch5[4]->Queue(1000)->[0]client5;
client5[0]->[4]switch5;

switch6[3]->[0]gateway6;
gateway6[0]->[3]switch6;

FromDevice(eth0)->HostEtherFilter(84:2b:2b:42:f9:e5, DROP_OWN true)->cl6::Classifier(12/0800, -);
cl6[1]->Discard;
cl6[0]->Strip(14)->Print("Gateway6 ->")->[0]encap6;
encap6[0]->Print("Encap6 ->")->[1]gateway6;
gateway6[1]->[1]encap6;

gateway6[2]->Print("Gateway6 <-")->[0]decap6->Print("Decap6 ->")->EtherEncap(0x0800, 84:2b:2b:42:f9:e5, ff:ff:ff:ff:ff:ff)->Queue->ToDevice(eth0, METHOD LINUX);

switch7[3]->[0]gateway7;
gateway7[0]->[3]switch7;

FromDevice(eth1)->HostEtherFilter(84:2b:2b:42:f9:e6, DROP_OWN true)->cl7::Classifier(12/0800, -);
cl7[1]->Discard;
cl7[0]->Strip(14)->Print("Gateway7 ->")->[0]encap7;
encap7[0]->Print("Encap7 ->")->[1]gateway7;
gateway7[1]->[1]encap7;

gateway7[2]->Print("Gateway7 <-")->[0]decap7->Print("Decap7 ->")->EtherEncap(0x0800, 84:2b:2b:42:f9:e6, 00:1b:21:48:09:34)->Print("EtherEncap7")->Queue->ToDevice(eth1, METHOD LINUX);


certserver5[0]->[5]switch5;
switch5[5]->Queue(1000)->[0]certserver5;
certserver6[0]->[4]switch6;
switch6[4]->Queue(1000)->[0]certserver6;
certserver7[0]->[4]switch7;
switch7[4]->Queue(1000)->[0]certserver7;

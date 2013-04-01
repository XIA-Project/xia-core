require(library xia_address.click);
require(library xia_xion_lib.click);

ad2_host :: XIAEndHost (RE AD2 HID2, HID2, 3002, 0, aa:aa:aa:aa:aa:aa);
ad3_host :: XIAEndHost (RE AD3 HID3, HID3, 3003, 1, aa:aa:aa:aa:aa:aa);

ad1xionrouter :: XionCoreDualStackRouter2XIA2SCION(
  RE AD1 RHID1, AD1, RHID1, 0.0.0.0, 2001, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa,
  1,
  "./TD1/TDC/AD1/AD1.conf",
  "./TD1/TDC/AD1/topology1.xml",
  "./TD1/TDC/AD1/certserver/ROT/rot-0.xml",
  "./TD1/TDC/AD1/certserver/privatekey/td1-ad1-0.key",
  "./TD1/TDC/AD1/certserver/certificates/td1-ad1-0.crt",
  "./TD1/TDC/AD1/AD1R1.conf",
  "./TD1/TDC/AD1/AD1R2.conf");

ad2xionrouter :: XionDualStackRouter2XIA1SCION(
  RE AD2 RHID2, AD2, RHID2, 0.0.0.0, 2002, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa,
  2,
  "./TD1/Non-TDC/AD2/AD2.conf",
  "./TD1/Non-TDC/AD2/topology2.xml",
  "./TD1/Non-TDC/AD2/certserver/ROT/rot-0.xml",
  "./TD1/Non-TDC/AD2/certserver/privatekey/rot-0.xml",
  "./TD1/Non-TDC/AD2/certserver/certificates/td1-ad2-0.crt",
  "./TD1/Non-TDC/AD2/AD2R1.conf");

ad3xionrouter :: XionDualStackRouter2XIA1SCION(
  RE AD3 RHID3, AD3, RHID3, 0.0.0.0, 2003, aa:aa:aa:aa:aa:aa, aa:aa:aa:aa:aa:aa,
  3,
  "./TD1/Non-TDC/AD3/AD3.conf",
  "./TD1/Non-TDC/AD3/topology3.xml",
  "./TD1/Non-TDC/AD3/certserver/ROT/rot-0.xml",
  "./TD1/Non-TDC/AD3/certserver/privatekey/rot-0.xml",
  "./TD1/Non-TDC/AD3/certserver/certificates/td1-ad3-0.crt",
  "./TD1/Non-TDC/AD3/AD3R1.conf");

Script(write ad2_host/xrc/n/proc/rt_XION.initialize HOSTMODE DEFAULTPORT 0);
Script(write ad3_host/xrc/n/proc/rt_XION.initialize HOSTMODE DEFAULTPORT 0);
Script(write ad2_host/xrc/n/proc/rt_XION_UNRESOLV.initialize HOSTMODE DEFAULTPORT 0);
Script(write ad3_host/xrc/n/proc/rt_XION_UNRESOLV.initialize HOSTMODE DEFAULTPORT 0);
// native XION routing
Script(write ad1xionrouter/xrc/n/proc/rt_XION.initialize ADMODE OFGKEY 1234567890);
Script(write ad2xionrouter/xrc/n/proc/rt_XION.initialize ADMODE OFGKEY 2345678901);
Script(write ad3xionrouter/xrc/n/proc/rt_XION.initialize ADMODE OFGKEY 3456789012);
// SCION encap routing (as 4id does for ip)
Script(write ad1xionrouter/xrc/n/proc/rt_XION_UNRESOLV.initialize HOSTMODE DEFAULTPORT 2);
Script(write ad2xionrouter/xrc/n/proc/rt_XION_UNRESOLV.initialize HOSTMODE DEFAULTPORT 2);
Script(write ad3xionrouter/xrc/n/proc/rt_XION_UNRESOLV.initialize HOSTMODE DEFAULTPORT 2);

Script(write ad1xionrouter/xrc/n/proc/rt_XION.add_port2ifid -2:0 0:10 1:11);
Script(write ad2xionrouter/xrc/n/proc/rt_XION.add_port2ifid -2:0 0:0 1:20);
Script(write ad3xionrouter/xrc/n/proc/rt_XION.add_port2ifid -2:0 0:0 1:30);

// XIA Plane connection
      ad2_host[0]       -> LinkUnqueue(0.005, 1 GB/s)
-> [0]ad2xionrouter[0]  -> LinkUnqueue(0.005, 1 GB/s)
-> [0]ad2_host;
      ad3_host[0]       -> LinkUnqueue(0.005, 1 GB/s)
-> [0]ad3xionrouter[0]  -> LinkUnqueue(0.005, 1 GB/s)
-> [0]ad3_host;

      ad1xionrouter[0] -> LinkUnqueue(0.005, 1 GB/s)
-> [1]ad2xionrouter[1] -> LinkUnqueue(0.005, 1 GB/s)
-> [0]ad1xionrouter;
      ad1xionrouter[1] -> LinkUnqueue(0.005, 1 GB/s)
-> [1]ad3xionrouter[1] -> LinkUnqueue(0.005, 1 GB/s)
-> [1]ad1xionrouter;

// SCION Plane connection
      ad1xionrouter[2] -> [2]ad2xionrouter[2] -> [2]ad1xionrouter;
      ad1xionrouter[3] -> [2]ad3xionrouter[2] -> [3]ad1xionrouter;

ControlSocket(tcp, 7777);

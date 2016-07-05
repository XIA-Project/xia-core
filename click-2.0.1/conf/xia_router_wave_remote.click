require(library xia_router_lib.click);
require(library xia_address.click);

log::XLog(VERBOSE 1, LEVEL 7);

// router instantiation
mg :: XIARouter2Port(RE AD:114bddc3ba7f99d8f07d359211a18af8fb5542b7 HID:9cc5682effe3522863a413f54cc613c9eaec3aff, AD:114bddc3ba7f99d8f07d359211a18af8fb5542b7, HID:9cc5682effe3522863a413f54cc613c9eaec3aff, 0.0.0.0, 1500, 00:26:AD:01:ED:7C, 00:00:00:00:00:00);

waveDevice :: WaveDeviceRemote(HOSTNAME 192.168.0.1, PORT 45622, CHANNEL 172);

waveDevice -> [0]mg[0] -> waveDevice;
Idle -> [1]mg[1] -> Discard;

ControlSocket(tcp, 7777);

AlignmentInfo(mg/xrc/c 4 0, mg/xlc0/c 4 0, mg/xlc0/statsFilter 4 0, mg/xlc1/c 4 0, mg/xlc1/statsFilter 4 0);

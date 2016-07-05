require(library xia_router_lib.click);
require(library xia_address.click);

log::XLog(VERBOSE 0, LEVEL 7);

// host instantiation
mg :: XIAEndHost (RE AD_INIT HID:493ae895f207c591dcaeb0a849a7f9d441d4ff5e, HID:493ae895f207c591dcaeb0a849a7f9d441d4ff5e, 1500, 0, 00:26:AD:01:ED:7C);

waveDevice :: WaveDeviceLocal(ROLE user, CHANNEL 172);

waveDevice -> [0]mg[0] -> waveDevice;


ControlSocket(tcp, 7777);

AlignmentInfo(mg/xrc/c 4 0, mg/xlc/c 4 0, mg/xlc/statsFilter 4 0); 


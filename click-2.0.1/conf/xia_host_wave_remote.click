require(library xia_router_lib.click);
require(library xia_address.click);

log::XLog(VERBOSE 1, LEVEL 7);

// host instantiation
mg :: XIAEndHost (RE AD_INIT HID:493ae895f207c591dcaeb0a849a7f9d441d4ff5e, HID:493ae895f207c591dcaeb0a849a7f9d441d4ff5e, 1500, 0, 00:26:AD:01:F6:AE);

waveDeviceRemote :: WaveDeviceRemote(HOSTNAME 192.168.0.2, PORT 45622, CHANNEL 172);

waveDeviceRemote -> [0]mg[0] -> waveDeviceRemote;


ControlSocket(tcp, 7777);

require(library xia_router_lib.click);

elementclass XionCoreDualStackRouter2XIA2SCION {
  $local_addr, $local_ad, $local_hid, $external_ip, $click_port, $mac0, $mac1,
  $xion_adid, $xion_config, $xion_topology, $xion_rot, $xion_pkey, $xion_cert, $xion_router1_conf, $xion_router2_conf |

  switch :: NSCIONSwitch(CONFIG_FILE $xion_config,
                         TOPOLOGY_FILE $xion_topology);
  beacon_server_core :: SCIONBeaconServerCore(AID 11111,
                                              CONFIG_FILE $xion_config,
                                              TOPOLOGY_FILE $xion_topology,
                                              ROT $xion_rot);
  path_server_core :: SCIONPathServerCore(AID 22222,
                                          CONFIG $xion_config,
                                          TOPOLOGY $xion_topology);
  cert_server_core :: SCIONCertServerCore(AID 33333,
                                          TOPOLOGY_FILE $xion_topology,
                                          CONFIG_FILE $xion_config,
                                          ROT $xion_rot,
                                          PrivateKey $xion_pkey,
                                          Cert $xion_cert);
  xion_bridge :: XIONBridge(AID 44444, ADID $xion_adid, TOPOLOGY_FILE $xion_topology);
  edge_router1 :: NSCIONRouter(AID 1111,
                               CONFIG_FILE $xion_router1_conf,
                               TOPOLOGY_FILE $xion_topology);
  edge_router2 :: NSCIONRouter(AID 2222,
                               CONFIG_FILE $xion_router2_conf,
                               TOPOLOGY_FILE $xion_topology);

  beacon_server_core -> [0]switch[0] -> Queue -> beacon_server_core;
  path_server_core   -> [1]switch[1] -> Queue -> path_server_core;
  cert_server_core   -> [2]switch[2] -> Queue -> cert_server_core;
  edge_router1[0]    -> [3]switch[3] -> [0]edge_router1;
  edge_router2[0]    -> [4]switch[4] -> [0]edge_router2;
  xion_bridge[0]     -> [5]switch[5] -> [0]xion_bridge;

  xrc :: XIARoutingCore($local_addr, $local_hid, $external_ip, $click_port, 3, 0);
  xlc0 :: XIALineCard($local_addr, $local_hid, $mac0, 0);
  xlc1 :: XIALineCard($local_addr, $local_hid, $mac0, 1);

  input[0,1,2,3] => xlc0, xlc1, [1]edge_router1[1], [1]edge_router2[1] => [0,1,2,3]output;
  xrc -> XIAPaintSwitch[0,1,2] => [1]xlc0[1], [1]xlc1[1], [1]xion_bridge[1] -> [0]xrc;
}

elementclass XionDualStackRouter2XIA1SCION {
  $local_addr, $local_ad, $local_hid, $external_ip, $click_port, $mac0, $mac1,
  $xion_adid, $xion_config, $xion_topology, $xion_rot, $xion_pkey, $xion_cert, $xion_router_conf |
  
  switch :: NSCIONSwitch(CONFIG_FILE $xion_config,
                         TOPOLOGY_FILE $xion_topology);
  beacon_server :: SCIONBeaconServer(AID 11111,
                                     CONFIG_FILE $xion_config,
                                     TOPOLOGY_FILE $xion_topology,
                                     ROT $xion_rot);
  path_server :: SCIONPathServer(AID 22222,
                                 CONFIG $xion_config,
                                 TOPOLOGY $xion_topology);
  cert_server :: SCIONCertServer(AID 33333,
                                 TOPOLOGY_FILE $xion_topology,
                                 CONFIG_FILE $xion_config,
                                 ROT $xion_rot,
                                 PrivateKey $xion_pkey,
                                 Cert $xion_cert);
  xion_bridge :: XIONBridge(AID 44444, ADID $xion_adid, TOPOLOGY_FILE $xion_topology);
  edge_router :: NSCIONRouter(AID 1111,
                              CONFIG_FILE $xion_router_conf,
                              TOPOLOGY_FILE $xion_topology);

  beacon_server  -> [0]switch[0] -> Queue -> beacon_server;
  path_server    -> [1]switch[1] -> Queue -> path_server;
  cert_server    -> [2]switch[2] -> Queue -> cert_server;
  edge_router[0] -> [3]switch[3] -> [0]edge_router;
  xion_bridge[0] -> [4]switch[4] -> [0]xion_bridge;

  xrc :: XIARoutingCore($local_addr, $local_hid, $external_ip, $click_port, 3, 0);
  xlc0 :: XIALineCard($local_addr, $local_hid, $mac0, 0);
  xlc1 :: XIALineCard($local_addr, $local_hid, $mac0, 1);

  input[0,1,2] => xlc0, xlc1, [1]edge_router[1] => [0,1,2]output;
  xrc -> XIAPaintSwitch[0,1,2] => [1]xlc0, [1]xlc1, [1]xion_bridge;
  xlc0[1] -> [0]xrc;
  xlc1[1] -> [0]xrc;
  xion_bridge[1] -> MarkXIAHeader -> XIAPaint(2) -> XIANextHop -> [0]xrc;
}

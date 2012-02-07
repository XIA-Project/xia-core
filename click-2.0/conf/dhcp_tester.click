elementclass XIAHost {
  $eth_fake_name, $eth_addr, $CLICK_IP, $API_IP, $xia_pub_key, $xia_cache_enable |

  // elements
  eth_fake :: FromHost($eth_fake_name,
                       $API_IP/24,
                       CLICK_XTRANSPORT_ADDR $CLICK_IP);
  eth_fake_classifier :: Classifier(12/0806,
                                    12/0800);
  output_queue :: Queue(200);

  // APPLICATION SIDE
  eth_fake -> eth_fake_classifier;
  eth_fake_classifier[0] -> ARPResponder(0.0.0.0/0 $eth_addr) -> output_queue;
  eth_fake_classifier[1] -> output_queue;

  // NETWORK SIDE
  input -> ToHost($eth_fake_name);
}

h1 :: XIAHost(eth_fake0, 11:11:11:11:11:11, 192.0.0.2, 192.0.0.1, 0, 0);
h2 :: XIAHost(eth_fake1, 22:22:22:22:22:22, 172.0.0.2, 172.0.0.1, 0, 0);

h1 -> Print(h1->h2) -> h2;
h2 -> Print(h2->h1) -> h1;

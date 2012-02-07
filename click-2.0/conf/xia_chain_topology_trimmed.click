require(library xia_template_trimmed.click)

host1 :: XIAHost(fakehost1,
                 11:11:11:11:11:11,
                 192.0.0.2,
                 192.0.0.1,
                 0)
host2 :: XIAHost(fakehost1,
                 11:11:11:11:11:12,
                 172.0.0.2,
                 172.0.0.1,
                 0)

host1 -> host2;
host2 -> host1;

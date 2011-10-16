The only changes made into the bind right now is
  - addition of xia_65532.c and xia_65532.h at
    bind-9.8.1/lib/dns/rdata/generic

XIA DAG RR naively stores dag string, nothing special
using type number 65532 <- temporary

configure, make, make install
no extra configuration required

sample setup in bind_sample_conf folder
  - named.conf -> /etc/named.conf
  - example.com.zone -> /var/example.com.zon
  - localhost.zone -> /var/localhost.zone

testing getxiadstdagbyname() <- for getting xia destination DAG
make XIAResolver/
run XIAResolver/tester
type 'example.com' for testing <- gets dns RR from local dns server
                                  (127.0.0.1 #53)

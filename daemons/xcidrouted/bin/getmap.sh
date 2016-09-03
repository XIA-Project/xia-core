#!/bin/bash

id2cid_host1=zihaol1@pc5.instageni.northwestern.edu
id2cid_port1=30266
id2cid_host2=zihaol1@pc5.instageni.northwestern.edu
id2cid_port2=30267
id2cid_host3=zihaol1@pc2.instageni.northwestern.edu
id2cid_port3=30266
id2cid_host4=zihaol1@pc2.instageni.northwestern.edu
id2cid_port4=30267

id2cid_hosts=($id2cid_host1 $id2cid_host2 $id2cid_host3 $id2cid_host4)
id2cid_ports=($id2cid_port1 $id2cid_port2 $id2cid_port3 $id2cid_port4)

touch tmp/id2cid_s1.dat

for i in "${!id2cid_hosts[@]}"; do 
	id2cid_host=${id2cid_hosts[$i]}
	id2cid_port=${id2cid_ports[$i]}

	scp -P $id2cid_port $id2cid_host:~/xia-core/daemons/xcidrouted/tmp/* tmp/temp
	cat tmp/temp >> tmp/id2cid_s1.dat
	rm -f tm/temp
done
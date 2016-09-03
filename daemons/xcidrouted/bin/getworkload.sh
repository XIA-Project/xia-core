#!/bin/bash

workload_port=31546
workload_host=zihaol1@pc4.instageni.stanford.edu

scp -P $workload_port $workload_host:~/xia-core/daemons/xcidrouted/workload/* workload/.
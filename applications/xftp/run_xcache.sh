#!/bin/sh

kill $(ps aux | grep '/bin/xcache' | awk '{print $2}')
~/xia-core/bin/xcache -h host1 -v -l7&
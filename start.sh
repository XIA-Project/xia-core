#!/bin/sh
rm etc/click/templates/host.click
rm etc/click/templages/router.click
sudo rm -rf /tmp/content/*
if [ "$1" = "host" ]; then
   sudo ./bin/xianet -t -I eth1 ${2}
fi
if [ "$1" = "router" ]; then
   sudo ./bin/xianet -nr ${2}
fi


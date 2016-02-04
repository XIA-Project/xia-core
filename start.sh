#!/bin/sh
if [ "$1" = "host" ]; then
   sudo ./bin/xianet -t -I eth4 ${2}
fi
if [ "$1" = "router" ]; then
   sudo ./bin/xianet -nr ${2}
fi


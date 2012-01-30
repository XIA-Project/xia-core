#!/bin/bash
#
# run click using the xtransport module (datagram/chunk/stream support)
#

NAME=`basename $0`
CLICK=../click-2.0/userlevel/click
SCRIPT=../click-2.0/conf/xia_chain_topology_socket_reliable_test.click
VERBOSE="0"

# this is the old script that used the xudp module (no stream support)
#SCRIPT=../click-2.0/conf/xia_chain_topology_socket.click

help ()
{
	cat << EOH

usage: $NAME [-qv]
where:
  -q runs click silently (default)
  -v prints click debug messages to stdout

EOH
	exit 0
}

while getopts ":qvh" opt; do
	case $opt in
		q)
			VERBOSE="0"
			;;
		v)
			VERBOSE="1"
			;;
		h)
			help
			;;
		\?)
			printf "\nInvalid option: -$OPTARG\n" >&2
			help
			;;
	esac
done

if [ $VERBOSE -eq "0" ]; then
	exec &> /dev/null
fi

sudo $CLICK $SCRIPT

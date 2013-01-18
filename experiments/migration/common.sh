# change the username if not available
USER=hlim

COMMON_PREFIX=/mnt/home-xia-router0/$USER/xia-core/migration
IMAGE_PATH=`ls $COMMON_PREFIX/ubuntu-kvm/*.qcow2`

COMMON_ARGS="
	-m 128
	-smp 1
	-drive file=$IMAGE_PATH
	-monitor tcp:0.0.0.0:4444,server,nowait
	-net nic,vlan=0
	-net user,vlan=0,hostfwd=tcp:0.0.0.0:5555-:22
	-net nic,vlan=1,macaddr=00:11:22:33:44:55
	-net tap,vlan=1,ifname=tap0,script=$COMMON_PREFIX/tap-up.sh,downscript=$COMMON_PREFIX/tap-down.sh
	-nographic"
INCOMING_ARGS="-incoming tcp:0.0.0.0:6666"

HOST_A_IP="128.2.208.168"
HOST_B_IP="128.2.208.169"

if [ "`hostname`" == "xia-router0" ]; then
	CLICK_PATH=/mnt/home-xia-router0/$USER/xia-core/click-2.0
elif [ "`hostname`" == "xia-router1" ]; then
	CLICK_PATH=/mnt/home-xia-router0/$USER/xia-core/click-2.0
else
	CLICK_PATH=~/xia-core/click-2.0
fi


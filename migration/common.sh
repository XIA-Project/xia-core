COMMON_PREFIX=/mnt/home-xia-router0/hlim/xia-core/migration
IMAGE_PATH=`ls $COMMON_PREFIX/ubuntu-kvm/*.qcow2`

COMMON_ARGS="-m 128 -smp 1 -drive file=$IMAGE_PATH -monitor tcp:0.0.0.0:4444,server,nowait -net nic -net user,hostfwd=tcp:0.0.0.0:5555-:22 -net tap,ifname=tap0,script=no -nographic"
INCOMING_ARGS="-incoming tcp:0.0.0.0:6666"

HOST_A_IP="128.2.208.168"
HOST_B_IP="128.2.208.169"


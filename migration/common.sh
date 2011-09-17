FILENAME=`ls $PREFIX/ubuntu-kvm/*.qcow2`
#TAP_ARGS="-net tap,name=eth1,ifname=tap0,script=no"
TAP_ARGS=""
COMMON_ARGS="-m 128 -smp 1 -drive file=$FILENAME -monitor tcp:127.0.0.1:4444,server,nowait -net nic -net user,hostfwd=tcp:127.0.0.1:5555-:22 $TAP_ARGS -nographic"
LISTEN_MODE_ARGS="-incoming tcp:0:6666"
HOST_A_IP="128.2.208.168"
HOST_B_IP="128.2.208.169"

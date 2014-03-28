#!/usr/bin/env bash
echo "Starting XIA..."
./bin/xianet kill &> /dev/null
rm TD1/TDC/AD1/beaconserver/logs/AD1BS.log
rm TD1/Non-TDC/AD2/beaconserver/logs/AD2BS.log
rm TD1/Non-TDC/AD3/beaconserver/logs/AD3BS.log
./configure &> /dev/null
make -j2 &> /dev/null
clear &> /dev/null
./bin/xianet -s xia_scion_topology.click -V start &> /dev/null
echo "XIA started."

echo "Monitoring beacon servers..."
while true
do
    sleep 5

    ad1=`tail TD1/TDC/AD1/beaconserver/logs/AD1BS.log | grep -i 'sent'`
    ad2=`tail TD1/Non-TDC/AD2/beaconserver/logs/AD2BS.log | grep -i 'passed'`
    ad3=`tail TD1/Non-TDC/AD3/beaconserver/logs/AD3BS.log | grep -i 'passed'`

    if [ "$ad1" == "" -o "$ad2" == "" -o "$ad3" == "" ]
    then
        echo "."
    else
        echo "PCB successfully propagated and verified."
        ./bin/xianet kill &> /dev/null
        exit
    fi
done

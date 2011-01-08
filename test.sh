cd click
make
cd ..
./click/userlevel/click ./click/conf/mytest.click &
sleep 2
python ./proxies/proxy.py 10000 &
python ./proxies/server.py &


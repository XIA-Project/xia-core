#!/bin/bash

cd ~
wget https://raw.github.com/XIA-Project/xia-core/develop/experiments/planetlab/id_rsa
chmod 600 id_rsa
mkdir .ssh
mv id_rsa .ssh/
echo "
cd ~
mkdir fedora-bin
cd fedora-bin
rsync --rsh='ssh -o StrictHostKeyChecking=no -p5556' update@gs11698.sp.cs.cmu.edu:~/fedora/xia-core/experiments/planetlab/bin-files ./
rsync --rsh='ssh -o StrictHostKeyChecking=no -p5556' -a --files-from=./bin-files update@gs11698.sp.cs.cmu.edu:~/fedora/ ./
./xia-core/experiments/planetlab/refresh.sh
" > sync.sh
chmod 755 sync.sh
./sync.sh
#echo '1 * * * * /home/cmu_xia/sync.sh' | crontab -


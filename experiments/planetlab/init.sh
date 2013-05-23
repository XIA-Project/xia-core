#!/bin/bash

cd ~
curl https://raw.github.com/XIA-Project/xia-core/develop/experiments/planetlab/id_rsa > id_rsa
chmod 600 id_rsa
mkdir .ssh
mv id_rsa .ssh/
echo "export PATH=\"~/fedora-bin/xia-core/bin:\$PATH\"" > .bash_profile
echo "alias pl='cd ~/fedora/xia-core/experiments/planetlab/'" >> .bash_profile
echo "
cd ~
mkdir fedora-bin
cd fedora-bin
until rsync --rsh='ssh -o StrictHostKeyChecking=no -p5556' update@gs11698.sp.cs.cmu.edu:~/fedora/xia-core/experiments/planetlab/bin-files ./; do echo \"retrying rsync\"; done
until rsync --rsh='ssh -o StrictHostKeyChecking=no -p5556' -ar --files-from=./bin-files update@gs11698.sp.cs.cmu.edu:~/fedora/ ./; do echo \"retrying rsync\"; done
./xia-core/experiments/planetlab/refresh.sh
" > sync.sh
chmod 755 sync.sh
./sync.sh
#echo '1 * * * * /home/cmu_xia/sync.sh' | crontab -


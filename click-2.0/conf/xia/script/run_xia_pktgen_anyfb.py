#!/usr/bin/python

import os
import sys

if len(sys.argv) < 3:
    print 'usage: %s NUM-FALLBACKS PAYLOAD-SIZE [COUNT]' % sys.argv[0]
    sys.exit(1)

nfallbacks = int(sys.argv[1])
payload_size = int(sys.argv[2])

script_names = {
    0: 'xia_mq_24_rand.click',
    1: 'xia_mq_24_fb1_rand.click',
    2: 'xia_mq_24_fb2_rand.click',
    3: 'xia_mq_24_fb3_rand.click',
}

dir_ = os.path.dirname(os.path.abspath(sys.argv[0]))

if len(sys.argv) >= 4:
    count = 'COUNT=%d' % int(sys.argv[3])
else:
    count = ''

cmd = '%s/../../../userlevel/click PAYLOAD_SIZE=%d %s -j12 %s/../userlevel/%s' % (dir_, payload_size, count, dir_, script_names[nfallbacks])

print(cmd)

ret = os.system(cmd)

sys.exit(ret)


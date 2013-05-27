#!/usr/bin/python

from subprocess import Popen, PIPE

def check_output(args):
    p = Popen(args,shell=True,stdout=PIPE,stderr=PIPE)
    out = p.communicate()
    rc = p.wait()
    if rc is not 0:
        raise Exception("subprocess.CalledProcessError: Command '%s'" \
                            "returned non-zero exit status %s" % (args, rc))
    return out

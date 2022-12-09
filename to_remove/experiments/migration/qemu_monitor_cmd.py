#!/usr/bin/python

import socket
import sys

def handle_cmd(monitor_addr, cmd):
    soc = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    soc.connect(monitor_addr)

    # wait for prompt
    resp = ''
    while True:
        if resp.endswith('(qemu) '):
            break
        resp += soc.recv(4096)

    # send command
    soc.sendall(cmd + '\r\n')

    # get result
    resp = ''
    while True:
        if resp.find('\r\n') != -1:
            resp = resp[resp.find('\r\n') + 2:]
            break
        resp += soc.recv(4096)
    while True:
        if resp.find('(qemu) ') != -1:
            resp = resp[:resp.find('(qemu) ')]
            break
        resp += soc.recv(4096)

    soc.close()

    return resp;


if __name__ == '__main__':
    if len(sys.argv) != 4:
        print 'usage: %s ADDR PORT CMD' % sys.argv[0]
        sys.exit(1)

    monitor_addr = (sys.argv[1], int(sys.argv[2]))
    cmd = sys.argv[3]

    resp = handle_cmd(monitor_addr, cmd)
    sys.stdout.write(resp)
    sys.exit(0)


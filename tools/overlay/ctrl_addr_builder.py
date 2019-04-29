import sys

def read_addrs_and_build_file(fd):
    ctrladdrs = {}
    lines = fd.readlines()
    for line in lines:
        line = line.strip()
        name, addr = line.split(',')
        ctrladdrs[name] = addr

    if fd is not sys.stdin:
        fd.close()

    return ctrladdrs

if __name__ == "__main__":
    ctrladdrs = read_addrs_and_build_file(sys.stdin)
    print ctrladdrs

import re
import subprocess

comment_pat = re.compile(r'^(#.*|No kallsyms.+|.+without symbols)$')
major_pat = re.compile(r'^([\.\d]+)%\s+(\S+)\s+(\S+)\s+\[(.+)\]\s+(.+)$')
#minor_sample_pat = re.compile(r'^([\.\d]+)$')
minor_sample_pat = re.compile(r'^([\.\d]+)%$')
minor_symbol_pat = re.compile(r'^(\S.*\S)$')
empty_pat = re.compile(r'^$')

"""Parses `perf report -g flat,0` output of a patched perf program and
collapses callgraph using stop symbols."""

verbose = False

def parse(seq, stop_symbols):
    it = seq.__iter__()
    line_no = 0

    last_sample = None
    callgraph = []

    stats = {}

    symbol_map = {}

    try:
        while True:
            line = it.next().strip()
            line_no += 1

            if comment_pat.match(line) is not None: continue


            mat = major_pat.match(line)
            if mat is not None: continue

            mat = minor_sample_pat.match(line)
            if mat is not None:
                #last_sample = int(mat.group(1))
                last_sample = float(mat.group(1))
                continue

            mat = minor_symbol_pat.match(line)
            if mat is not None:
                symbol = mat.group(1)
                #if symbol.startswith('0x') and not symbol.startswith('0x7f'):
                #    if symbol in symbol_map:
                #        symbol = symbol_map[symbol]
                #    else:
                #        # try to resolve the symbol
                #        cmd = 'addr2line -e ../../../userlevel/click -f -C %s' % symbol
                #        p = subprocess.Popen(cmd.split(' '), stdout=subprocess.PIPE)
                #        symbol_map[symbol] = resolved_symbol = p.stdout.readline().strip()
                #        p.wait()
                #        symbol = resolved_symbol
                if last_sample is not None:
                    callgraph.append(symbol)
                    for pat in stop_symbols:
                        if pat.match(symbol):
                            stats[symbol] = stats.get(symbol, 0) + last_sample
                            last_sample = None
                            callgraph = []
                            break
                continue
            
            mat = empty_pat.match(line)
            if mat is not None:
                if last_sample is not None:
                    if not callgraph[-1].startswith('0x7f'):
                        if verbose:
                            print 'no stop symbol included: near line %d' % line_no
                            print '  ' + '\n  '.join(callgraph) + '\n'
                        symbol = '<>' + callgraph[0]
                        stats[symbol] = stats.get(symbol, 0) + last_sample
                    else:
                        symbol = '<>' + callgraph[0]
                        stats[symbol] = stats.get(symbol, 0) + last_sample
                    last_sample = None
                    callgraph = []
                continue

            assert False, 'cannot parse: %s' % line

    except StopIteration:
        pass

    assert last_sample is None
    assert not callgraph

    result = sorted(stats.items(), key=lambda x: x[1], reverse=True)
    return result


# stop symbols for click router
stop_symbols = []
f = open('../../../userlevel/elements.conf')
for line in f.readlines():
    mat = re.match(r'^.+-(.+)?$', line.strip())
    if mat is not None:
        pat = r'^%s::.+$' % re.escape(mat.group(1))
        stop_symbols.append(re.compile(pat))
f.close()
if verbose:
    print 'found %d elements for stop symbols' % len(stop_symbols)


if __name__ == '__main__':
    import sys
    f = open(sys.argv[1])
    verbose = True
    result = parse(f.readlines(), stop_symbols)
    for symbol, sample in result:
        print '%20d %s' % (sample, symbol)
    print sum(map(lambda x: x[1], result))


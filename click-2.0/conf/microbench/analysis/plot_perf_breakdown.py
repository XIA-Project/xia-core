from common import *
import perf_callgraph

import re

# task
tasks = ['Classification', 'Fast', 'Slow-Lookup', 'Slow-Other', 'Queueing', 'Misc', 'Mem Alloc/Copy', None]
ignored_tasks = ['Mem Alloc/Copy', None]
task_colors = ['0.2', '1', '0.6', '0.4', '0.8']

# symbol name to task mapping
symbol_task_mappings = [
    (re.compile(r'^Clone::.*$'), 'Mem Alloc/Copy'),
    (re.compile(r'^Discard::.*$'), 'Mem Alloc/Copy'),
    (re.compile(r'^<>_int_malloc$'), 'Mem Alloc/Copy'),
    (re.compile(r'^<>_int_free$'), 'Mem Alloc/Copy'),
    (re.compile(r'^<>__malloc$'), 'Mem Alloc/Copy'),
    (re.compile(r'^<>cfree$'), 'Mem Alloc/Copy'),
    (re.compile(r'^IPRandomize::.*$'), 'Mem Alloc/Copy'),
    (re.compile(r'^XIARandomize::.*$'), 'Mem Alloc/Copy'),
    (re.compile(r'^<>__drand48_iterate$'), 'Mem Alloc/Copy'),
    (re.compile(r'^<>nrand48$'), 'Mem Alloc/Copy'),
    (re.compile(r'^<>nrand48@plt$'), 'Mem Alloc/Copy'),
    (re.compile(r'^<>nrand48_r$'), 'Mem Alloc/Copy'),
    (re.compile(r'^AggregateCounter::.*$'), None),
    (re.compile(r'^PrintStats::.*$'), None),
    (re.compile(r'^AddressInfo::.*$'), None),
    (re.compile(r'^InfiniteSource::.*$'), None),
    (re.compile(r'^XIAEncap::.*$'), None),
    (re.compile(r'^IPEncap::.*$'), None),

    (re.compile(r'^Unqueue::.*$'), 'Queueing'),
    (re.compile(r'^FullNoteQueue::.*$'), 'Queueing'),
    (re.compile(r'^NotifierQueue::.*$'), 'Queueing'),

    (re.compile(r'^XIAXIDTypeClassifier::.*$'), 'Classification'),

    (re.compile(r'^Paint::.*$'), 'Slow-Other'),
    (re.compile(r'^PaintSwitch::.*$'), 'Slow-Other'),
    (re.compile(r'^RangeIPLookup::.*$'), 'Slow-Lookup'),
    (re.compile(r'^RadixIPLookup::.*$'), 'Slow-Lookup'),
    (re.compile(r'^DirectIPLookup::.*$'), 'Slow-Lookup'),
    (re.compile(r'^DecIPTTL::.*$'), 'Slow-Other'),
    (re.compile(r'^IPFragmenter::.*$'), 'Slow-Other'),
    (re.compile(r'^XIACheckDest::.*$'), 'Slow-Other'),
    (re.compile(r'^XIAXIDRouteTable::.*$'), 'Slow-Lookup'),
    (re.compile(r'^XIASelectPath::.*$'), 'Slow-Other'),
    (re.compile(r'^XIADecHLIM::.*$'), 'Slow-Other'),
    (re.compile(r'^XIANextHop::.*$'), 'Slow-Other'),
    (re.compile(r'^Element::push\(.*\)$'), 'Slow-Other'),

    (re.compile(r'^XIAFastPath::.*$'), 'Fast'),

    #(re.compile(r'^XIARouterCache::.*$'), 'Slow-Other'),

    (re.compile(r'^<>__memset_sse2$'), 'Mem Alloc/Copy'),
    (re.compile(r'^<>memcpy$'), 'Mem Alloc/Copy'),
    (re.compile(r'^<>memcpy@plt$'), 'Mem Alloc/Copy'),
    (re.compile(r'^Packet::clone\(.*\)$'), 'Mem Alloc/Copy'),
    (re.compile(r'^Packet::expensive_uniqueify\(.*\)$'), 'Mem Alloc/Copy'),
    (re.compile(r'^Packet::alloc_data\(.*\)$'), 'Mem Alloc/Copy'),
    (re.compile(r'^Packet::~Packet\(.*\)$'), 'Mem Alloc/Copy'),
    (re.compile(r'^WritablePacket::pool_allocate\(.*\)$'), 'Mem Alloc/Copy'),
    (re.compile(r'^WritablePacket::recycle\(.*\)$'), 'Mem Alloc/Copy'),
    (re.compile(r'^RouterThread::driver\(.*\)$'), None),
    (re.compile(r'^XIAPrint::.*$'), None),

    (re.compile(r'^<>0x7ff.*$'), None),
    (re.compile(r'^<>0xfff.*$'), None),
    (re.compile(r'^<>_Z.*$'), None),
    (re.compile(r'^<>.*$'), 'Slow-Other'),
    #(re.compile(r'^<>.*$'), None),
]


def plot(output, data_names):
    plot_data = {}
    sample_data = {}
    for task in tasks:
        plot_data[task] = {}
        sample_data[task] = {}

    processing_times = {}

    for data_name in data_names:
        for task in tasks:
            plot_data[task][data_name] = 0.
        processing_times[data_name] = []

        for iter_i in range(0, iter_max):
            for task in tasks:
                sample_data[task][data_name] = 0.

            user_time = get_total_runtime(dataset[data_name] + '_timing' + '_%d' % iter_i)
            processing_times[data_name].append(user_time * 1000000000 / packet)

            perf_results = perf_callgraph.parse(open(dataset[data_name] + '_perf' + '_%d' % iter_i).readlines(), perf_callgraph.stop_symbols)

            for symbol, sample in perf_results:
                found = False

                for pat, task in symbol_task_mappings:
                    mat = pat.match(symbol)
                    if mat is None: continue

                    if task is None:
                        found = True
                        break

                    sample_data[task][data_name] += sample
                    found = True
                    break

                if not found:
                    assert False, 'cannot translate %s into a task name in %s iteration %d' % (symbol, dataset[data_name], iter_i)

            time_sum = 0
            for task in tasks:
                time_sum += sample_data[task][data_name]
            time_scale = processing_times[data_name][-1] / time_sum
            #print time_scale       # this should be similar across exps on an identical HW
            for task in tasks:
                plot_data[task][data_name] += sample_data[task][data_name] * time_scale / iter_max

    height = 0.6

    fig = plt.figure(figsize=(7, 7 * 0.6))
    fig.clear()
    ax = fig.add_subplot(111)

    yrange = len(data_names) - 1 - np.arange(len(data_names))

    ax.set_yticks(yrange + height / 2)
    ax.set_yticklabels(data_names)
    ax.set_xlabel('Processing time (ns)')

    last_left = [0] * len(data_names)

    for task in tasks:
        if task in ignored_tasks:
            continue

        widths = [0] * len(data_names)
        for data_name, ns in plot_data[task].items():
            widths[data_names.index(data_name)] = ns
        if sum(widths) == 0:
            continue

        ax.barh(yrange, widths, height=height, left=last_left, label=task, color=task_colors[tasks.index(task)], zorder = 3)

        for i in range(len(data_names)):
            last_left[i] += widths[i]

    ax.xaxis.grid(zorder=1)
    #x_max = max(last_left) * 1.16
    x_max = max(last_left) * 1.25
    ax.set_xlim(0, x_max)
    ax.set_ylim(-0.3, len(data_names) - 0.1)
    #ax.set_xticks(range(0, int(x_max), 200))
    ax.set_xticks(range(0, int(x_max), 100))
    ax.legend(loc='upper right')

    plt.savefig(output, format='pdf', bbox_inches='tight')

    print 'processing time'
    for i, data_name in enumerate(data_names):
        print '%-10s: %f (%f)' % (data_name, last_left[i], last_left[i] * 100. / last_left[0])


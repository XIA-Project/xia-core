from common import *
import perf_callgraph

import re

# task
tasks = ['I/O', 'Queueing', 'Classification', 'Routing', 'Misc']
task_colors = ['0.4', '0.6', '0.2', '0.8', '1']

# symbol name to task mapping
symbol_task_mappings = [
    (re.compile(r'^Clone::.*$'), 'I/O'),
    (re.compile(r'^Discard::.*$'), 'I/O'),
    (re.compile(r'^<>_int_malloc$'), 'I/O'),
    (re.compile(r'^<>_int_free$'), 'I/O'),
    (re.compile(r'^<>__malloc$'), 'I/O'),
    (re.compile(r'^<>cfree$'), 'I/O'),
    (re.compile(r'^IPRandomize::.*$'), 'I/O'),
    (re.compile(r'^XIARandomize::.*$'), 'I/O'),
    (re.compile(r'^<>__drand48_iterate$'), 'I/O'),
    (re.compile(r'^<>nrand48_r$'), 'I/O'),
    (re.compile(r'^AggregateCounter::.*$'), 'I/O'),
    (re.compile(r'^PrintStats::.*$'), 'I/O'),
    (re.compile(r'^AddressInfo::.*$'), 'I/O'),

    (re.compile(r'^Unqueue::.*$'), 'Queueing'),
    (re.compile(r'^FullNoteQueue::.*$'), 'Queueing'),

    (re.compile(r'^XIAXIDTypeClassifier::.*$'), 'Classification'),

    (re.compile(r'^Paint::.*$'), 'Routing'),
    (re.compile(r'^PaintSwitch::.*$'), 'Routing'),
    (re.compile(r'^RangeIPLookup::.*$'), 'Routing'),
    (re.compile(r'^RadixIPLookup::.*$'), 'Routing'),
    (re.compile(r'^DecIPTTL::.*$'), 'Routing'),
    (re.compile(r'^IPFragmenter::.*$'), 'Routing'),
    (re.compile(r'^XIACheckDest::.*$'), 'Routing'),
    (re.compile(r'^XIAXIDRouteTable::.*$'), 'Routing'),
    (re.compile(r'^XIASelectPath::.*$'), 'Routing'),
    (re.compile(r'^XIADecHLIM::.*$'), 'Routing'),
    (re.compile(r'^XIANextHop::.*$'), 'Routing'),

    #(re.compile(r'^XIARouterCache::.*$'), 'Content cache'),

    (re.compile(r'^<>.*$'), 'Misc'),
]


def plot(output, data_names):
    plot_data = {}
    for task in tasks:
        plot_data[task] = {}

    processing_times = {}

    for data_name in data_names:
        user_time = get_total_runtime(dataset[data_name] + '_timing')
        processing_times[data_name] = user_time * 1000000000 / packet

        perf_results = perf_callgraph.parse(open(dataset[data_name] + '_perf').readlines(), perf_callgraph.stop_symbols)

        for symbol, rate in perf_results:
            found = False

            for pat, task in symbol_task_mappings:
                mat = pat.match(symbol)
                if mat is None: continue

                if task is None:
                    found = True
                    break

                plot_data[task][data_name] = plot_data[task].get(data_name, 0) + rate
                found = True

            if not found:
                assert False, 'cannot translate %s into a task name' % symbol

        time_sum = 0
        for task in tasks:
            time_sum += plot_data[task].get(data_name, 0)
        time_scale = processing_times[data_name] / time_sum
        #print time_scale       # this should be similar across exps on an identical HW
        for task in tasks:
            plot_data[task][data_name] = plot_data[task].get(data_name, 0) * time_scale

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
        widths = [0] * len(data_names)
        for data_name, ns in plot_data[task].items():
            widths[data_names.index(data_name)] = ns
        if sum(widths) == 0:
            continue

        ax.barh(yrange, widths, height=height, left=last_left, label=task, color=task_colors[tasks.index(task)], zorder = 3)

        for i in range(len(data_names)):
            last_left[i] += widths[i]

    ax.xaxis.grid(zorder=1)
    ax.set_xlim(0, max(last_left) * 1.12)
    ax.set_ylim(-0.3, len(data_names) - 0.1)
    ax.legend(loc='upper right')

    plt.savefig(output, format='pdf', bbox_inches='tight')

    min_time = min(processing_times.values())
    print 'processing time'
    for data_name in data_names:
        print '%-10s: %f (%f)' % (data_name, processing_times[data_name], processing_times[data_name] * 100. / min_time)


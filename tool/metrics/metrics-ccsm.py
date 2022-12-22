#!/usr/bin/env python3

# requires https://github.com/bright-tools/ccsm

import os
import sys
import csv

folders = [
'src',
'src/ble',
'src/ble/gatt-service',
'src/classic',
]

metrics = {}
targets = {}

targets['PATH']   = 1000
targets['GOTO']   = 0
targets['CCN']    = 20
targets['CALLS']  = 12
targets['PARAM']  = 7
targets['STMT']   = 100
targets['LEVEL']  = 6
targets['RETURN'] = 1

excluded_functions = [
    # deprecated functions
    'src/l2cap.c:l2cap_le_register_service',
    'src/l2cap.c:l2cap_le_unregister_service',
    'src/l2cap.c:l2cap_le_accept_connection',
    'src/l2cap.c:l2cap_le_decline_connection',
    'src/l2cap.c:l2cap_le_provide_credits',
    'src/l2cap.c:l2cap_le_create_channel',
    'src/l2cap.c:l2cap_le_can_send_now',
    'src/l2cap.c:l2cap_le_request_can_send_now_event',
    'src/l2cap.c:l2cap_le_send_data',
    'src/l2cap.c:l2cap_le_disconnect',
    'src/l2cap.c:l2cap_cbm_can_send_now',
    'src/l2cap.c:l2cap_cbm_request_can_send_now_even'
]

def metric_sum(name, value):
    global metrics
    old = 0
    if name in metrics:
        old = metrics[name]
    metrics[name] = old + value


def metric_list(name, item):
    global metrics
    value = []
    if name in metrics:
        value = metrics[name]
    value.append(item)
    metrics[name] = value

def metric_max(name, max):
    global metrics
    if name in metrics:
        if metrics[name] > max:
            return
    metrics[name] = max

def metric_measure(metric_name, function_name, actual):
    metric_max(metric_name + '_MAX', actual)
    if metric_name in targets:
        metric_sum(metric_name + '_SUM', actual)
        if actual > targets[metric_name]:
            metric_sum(metric_name + '_DEVIATIONS', 1)
            # metric_list(metric_name + '_LIST', function_name + '(%u)' % actual)
            metric_list(metric_name + '_LIST', function_name)


def analyze_folders(btstack_root, folders):
    global excluded_functions

    # File,Name,"'goto' keyword count (raw source)","Return points","Statement count (raw source)(local)",
    # "Statement count (raw source)(cumulative)","Comment density","McCabe complexity (raw source)",
    # "Number of paths through the function","No. different functions called","Function Parameters",
    # "Nesting Level","VOCF","Number of functions which call this function",
    fields = [ 'file','function','GOTO','RETURN','_','STMT' ,'_','CCN','PATH','CALLS','PARAM','LEVEL','_','_','_']

    # init deviations
    for key in fields:
        metrics[key + '_DEVIATIONS'] = 0

    # for now, just read the file
    with open("metrics.tsv") as fd:
        rd = csv.reader(fd, delimiter="\t")
        last_function_name = ''
        for row in rd:
            file = ''
            function_metrics = {}
            for key, value in zip(fields, row):
                if key == 'file':
                    # get rid of directory traversal on buildbot
                    pos_metrics_folder = value.find('tool/metrics/')
                    if pos_metrics_folder > 0:
                        value = value[pos_metrics_folder+13:]
                    # streamline path
                    file = value.replace('../../','')
                    continue
                if key == 'function':
                    function_name = value
                    continue
                if key == '_':
                    continue
                function_metrics[key] = value
            if file.endswith('.h'):
                continue
            qualified_function_name = file+':'+function_name
            # excluded functions
            if qualified_function_name in excluded_functions:
                continue
            metric_sum('FUNC', 1)
            for key,value in function_metrics.items():
                metric_measure(key, qualified_function_name, int(function_metrics[key]))

def analyze(folders):
    # print ("\nAnalyzing:")
    # for path in folders:
    #     print('- %s' % path)
    #     analyze_folder(btstack_root + "/" + path)
    btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/../..')
    analyze_folders(btstack_root, folders)

def list_targets():
    print ("Targets:")
    for key,value in sorted(targets.items()):
        print ('- %-20s: %u' % (key, value))

def list_metrics():
    print ("\nResult:")
    num_funcs = metrics['FUNC']
    for key,value in sorted(metrics.items()):
        if key.endswith('LIST'):
            continue
        if key.endswith('_SUM'):
            average = 1.0 * value / num_funcs
            metric = key.replace('_SUM','_AVERAGE')
            print ('- %-20s: %4.3f' % (metric, average))
        else:
            print ('- %-20s: %5u' % (key, value))

def list_metrics_table():
    row = "%-11s |%11s |%11s |%11s"

    print( row % ('Name', 'Target', 'Deviations', 'Max value'))
    print("------------|------------|------------|------------")

    ordered_metrics = [ 'PATH', 'GOTO', 'CCN', 'CALLS', 'PARAM', 'STMT', 'LEVEL', 'RETURN', 'FUNC'];
    for metric_name in ordered_metrics:
        if metric_name in targets:
            target = targets[metric_name]
            deviations = metrics[metric_name + '_DEVIATIONS']
            max = metrics[metric_name + '_MAX']
            print ( row % ( metric_name, target, deviations, max))
        else:
            max = metrics[metric_name]
            print ( row % ( metric_name, '', '', max))

def list_deviations():
    global metrics
    for key,value in sorted(metrics.items()):
        if not key.endswith('LIST'):
            continue
        print ("\n%s" % key)
        print ('\n'.join(value))
 
analyze(folders)
list_metrics_table()
# list_targets()
# list_metrics()
# list_deviations()

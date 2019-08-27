#!/usr/bin/env python3

# requires https://github.com/terryyin/lizard

import lizard
import os
import sys

folders = [
'src',
'src/ble',
'src/ble/gatt-service',
'src/classic',
]

metrics = {}
targets = {}

targets['CCN']   = 10
targets['PARAM'] = 7

def metric_count(name):
    global metrics
    value = 0
    if name in metrics:
        value = metrics[name]
    metrics[name] = value + 1


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

def metric_measure(metric_name, functino_name, actual):
    metric_max(metric_name + '_MAX', actual)
    if metric_name in targets:
        if actual > targets[metric_name]:
            metric_count(metric_name + '_DEVIATIONS')
            metric_list(metric_name + '_LIST', functino_name)

def analyze_file(path):
    l = lizard.analyze_file(path)
    for f in l.function_list:
        metric_count('FUNC')
        metric_measure('PARAM', f.name, f.parameter_count)
        metric_measure('CCN',   f.name, f.cyclomatic_complexity )

def analyze_folder(folder_path):
    for file in sorted(os.listdir(folder_path)):
        if file.endswith(".c"):
            analyze_file(folder_path+'/'+file)

# find root
btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/../..')

print ("Targets:")
for key,value in sorted(targets.items()):
    print ('- %-20s: %u' % (key, value))

print ("\nAnalyzing:")
for path in folders:
    print('- %s' % path)
    analyze_folder(btstack_root + "/" + path)

print ("\nResult:")

for key,value in sorted(metrics.items()):
    if key.endswith('LIST'):
        continue
    print ('- %-20s: %4u' % (key, value))

for key,value in sorted(metrics.items()):
    if not key.endswith('LIST'):
        continue
    print ("\n%s" % key)
    print ('\n'.join(value))


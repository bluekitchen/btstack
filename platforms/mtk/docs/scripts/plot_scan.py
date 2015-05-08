#!/usr/bin/env python

import matplotlib.pyplot as plt
#from pylab import *
import cPickle
import pylab as P
import numpy as np
from matplotlib.backends.backend_pdf import PdfPages
from matplotlib.patches import Polygon
import itertools
import os



def histplot(data,labels, colors, x_label, y_label, title, fig_name, cdf):
    fig, ax = plt.subplots()
    if cdf:
        n, bins, patches = ax.hist(data, 20, weights=None, histtype='step', normed=True, cumulative=True, label= labels, color = colors)
        legend = ax.legend(loc='lower left', shadow=False)
        ax.grid(True)
    
    else:
        n, bins, patches = ax.hist( data, 20, weights=None, histtype='bar', label= labels, color = colors)
        legend = ax.legend(loc='upper right', shadow=False)

    for line in ax.get_lines():
        line.set_linewidth(1.5)

    ax.set_xlabel(x_label)
    ax.set_ylabel(y_label)
    for label in legend.get_texts():
        label.set_fontsize('small')

    for label in legend.get_lines():
        label.set_linewidth(1.5)  # the legend line width

    fig.suptitle(title, fontsize=12)
    
    #plt.show()
    pp = PdfPages(fig_name)
    pp.savefig(fig)
    pp.close()
    return [n, bins, patches]

def accplot(data, labels, colors, x_label, y_label, title, fig_name, annotation):
    mean = np.zeros(len(data))
    for i in range(len(data)):
        if len(data[i]) > 0: 
            mean[i] = len(data[i]) /(1.0*max(data[i]))
        
    mean = round(mean)

    fig, ax = plt.subplots()
    for i in range(len(data)):
        if len(data[i]) > 0: 
            ax.plot(data[i], range(len(data[i])), colors[i], label= labels[i]+', '+mean[i]+' adv/s, total nr. '+str(len(data[i])))
    
    ax.set_xlabel(x_label)
    ax.set_ylabel(y_label)
    for tl in ax.get_yticklabels():
        tl.set_color('k')

    legend = ax.legend(loc='upper left', shadow=False)
    
    for label in legend.get_texts():
        label.set_fontsize('small')

    for label in legend.get_lines():
        label.set_linewidth(1.5)  # the legend line width
    
    for line in ax.get_lines():
        line.set_linewidth(1.5)
    
    fig.suptitle(title, fontsize=12)
    ax.text(400, 5000, annotation , style='italic',
        bbox={'facecolor':'gray', 'alpha':0.5, 'pad':10})

    #plt.show()
    pp = PdfPages(fig_name)
    pp.savefig(fig)
    pp.close()

    return fig

def mean_common_len(data):
    mcl = 0
    for i in range(len(data) - 1):
        if len(data[i]) > 0:
            if mcl == 0:
                mcl = len(data[i])
            else:
                mcl = min(mcl, len(data[i]))
    return mcl

def mean_common_time(data):
    mct = 0
    for i in range(len(data) - 1):
        if len(data[i]) > 0:
            if mct == 0:
                mct = max(data[i])
            else:
                mct = min(mct, max(data[i]))
    return mct

def normalize(s): 
    return map(lambda x: (x - s[0]), s)

def delta(s): 
    rs = list()
    for i in range(len(s)-1):
        rs.append(s[i+1] - s[i])
    return rs

def round(s): 
    return map(lambda x: "{0:.4f}".format(x), s)

def cut(s, V): 
    r = list()
    for i in range(len(s)):
        if s[i] <= V:
            r.append(s[i])
    return r

def prepare_data(exp_name, sensor_name):
    prefix = '../data/processed/'
    
    scanning_type = exp_name+'_continuous'
    mn = cPickle.load(open(prefix+scanning_type+'_mac_'+sensor_name+'.data', 'rb')) # mac nio, 
    mm = cPickle.load(open(prefix+scanning_type+'_mac_mac.data', 'rb')) # mac mac, 
    rn = cPickle.load(open(prefix+scanning_type+'_rug_'+sensor_name+'.data', 'rb')) # ruggear nio, 
    rm = cPickle.load(open(prefix+scanning_type+'_rug_mac.data', 'rb')) # ruggear mac, 
    
    scanning_type = exp_name+'_normal'
    try:
        normal_rn = cPickle.load(open(prefix + scanning_type+'_rug_'+sensor_name+'.data', 'rb')) # ruggear mac, normal
    except:
        normal_rn = list()

    try:
        normal_mn = cPickle.load(open(prefix + scanning_type+'_mac_'+sensor_name+'.data', 'rb')) # ruggear mac, normal
    except:
        normal_mn = list()
    
    try:
        normal_rm = cPickle.load(open(prefix + scanning_type+'_rug_mac.data', 'rb')) # ruggear mac, normal
    except:
        normal_rm = list()

    try:
        normal_mm = cPickle.load(open(prefix + scanning_type+'_mac_mac.data', 'rb')) # ruggear mac, normal
    except:
        normal_mm = list()

        
    T  = mean_common_time([mm, mn, rm, rn, normal_rm, normal_rn, normal_mm, normal_mn])
    L  = mean_common_len([mm, mn, rm, rn, normal_rm, normal_rn, normal_mm, normal_mn])
    Z  = 15

    print "mct %d, mcl %d" % (T,L)
    mac_mac = normalize(mm)
    mac_nio = normalize(mn)
    ruggeer_mac = normalize(rm)
    ruggeer_nio = normalize(rn)
    
    ruggeer_nio_normal = normalize(normal_rn)
    ruggeer_mac_normal = normalize(normal_rm)
    mac_mac_normal = normalize(normal_mm)
    mac_nio_normal = normalize(normal_mn)

        
    delta_mn = delta(mac_nio)
    delta_mm = delta(mac_mac)
    delta_rn = delta(ruggeer_nio)
    delta_rm = delta(ruggeer_mac)

    rn_delays = list()
    for i in range(len(delta_rn)):
        rn_delays.append(range(delta_rn[i]))

    flattened_rn_delays = list(itertools.chain.from_iterable(rn_delays))

    plot_data = [cut(mac_mac,T), cut(mac_nio,T), cut(ruggeer_mac,T), cut(ruggeer_nio,T)]
    plot_data_normal = [cut(mac_mac_normal,T), cut(mac_nio_normal,T), cut(ruggeer_mac_normal,T), cut(ruggeer_nio_normal,T)]

    hist_data = [delta_mm[0:L], delta_mn[0:L], delta_rm[0:L], delta_rn[0:L]]
    
    zoomed_hist_data = list()
    if len(hist_data[0]) >= Z and len(hist_data[1]) >= Z and len(hist_data[2]) >= Z  and len(hist_data[3]) >= Z :
        zoomed_hist_data = [cut(hist_data[0],Z), cut(hist_data[1],Z), cut(hist_data[2],Z), cut(hist_data[3],Z)]

    return [plot_data, hist_data, zoomed_hist_data, flattened_rn_delays, plot_data_normal]

def plot(exp_name, sensor_name, sensor_title, prefix):
    [plot_data, hist_data, zoomed_hist_data, rn_delays, plot_data_normal] = prepare_data(exp_name, sensor_name)
    labels = ['Scan. BCM, Adv. BCM', 'Scan. BCM, Adv. '+ sensor_title, 'Scan. RugGear, Adv. BCM', 'Scan. RugGear, Adv. '+sensor_title]
    plot_colors = ['r-','k-','b-','g-']
    hist_colors = ['red','black','blue','green']

    title = 'Continuous scanning over time'
    annotation = 'scan window 30ms, scan interval 30ms'

    x_label = 'Time [s]'
    y_label = 'Number of advertisements'
    accplot(plot_data, labels, plot_colors, x_label, y_label, title, prefix+sensor_name+'_acc_number_of_advertisements_continuous_scanning.pdf', annotation)   

    x_label = 'Time interval between two advertisements [s]' 
    title = 'Continuous scanning - interval distribution'
    histplot(hist_data, labels, hist_colors, x_label, y_label, title, prefix+sensor_name+'_histogram_advertisements_time_delay.pdf', 0)

    
    #if len(zoomed_hist_data) > 0:
    #    title = 'Continuous scanning - interval distribution [0-15s]'
    #    histplot(zoomed_hist_data, labels, hist_colors, x_label, y_label, title, prefix+sensor_name+'_histogram_advertisements_time_delay_zoomed.pdf', 0)

    title = 'Continuous scanning - expected waiting time'
    x_label = 'Expected waiting time until first scan [s]' 
    [n, bins, patches] = histplot([rn_delays], [labels[3]], [hist_colors[3]], x_label, y_label, title, prefix+sensor_name+'_ruggear_expected_scan_response.pdf', 0)

    title = 'Continuous scanning - expected waiting time probability distribution'
    y_label = 'Advertisement probability'
    x_label = 'Time until first scan [s]' 
    [n, bins, patches] = histplot([rn_delays], [labels[3]], [hist_colors[3]], x_label, y_label, title, prefix+sensor_name+'_ruggear_cdf.pdf', 1)


    title = 'Normal scanning over time'
    annotation = 'scan window 30ms, scan interval 300ms'
    
    x_label = 'Time [s]'
    y_label = 'Number of advertisements'
    accplot(plot_data_normal, labels, plot_colors, x_label, y_label, title, prefix+sensor_name+'_acc_number_of_advertisements_normal_scanning.pdf', annotation)   


picts_folder = "../picts_experiments/"
if not os.access(picts_folder, os.F_OK):
    os.mkdir(picts_folder)

plot('exp1','nio', 'Nio', picts_folder)
plot('exp2','xg2', 'XG', picts_folder)

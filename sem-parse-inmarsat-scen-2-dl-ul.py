import sem
import os
import copy
import matplotlib.pyplot as plt
import tikzplotlib as tkz
import matplotlib as mpl
import numpy as np
import scienceplots
import itertools
from metrics_common import *

plt.style.use(['ieee', 'science'])
mpl.rcParams['legend.frameon'] = 'True'

ns_path = './'
ns_script = 'inmarsat-scenario-2'
ns_res_path = './results-inmarsat-scen-2-dl-ul'
CB_PATH_PREFIX_HOME = os.path.abspath(ns_path) + '/src/mmwave/model/Codebooks'

SRC_RATES = ['50Mbps','100Mbps']
RAIN_RATES = [0]
NUM_IABS = 5

SLOT_FORMATS = ['dddsu','ddddddsuuu']

SLOT_FORMATS_OUT = ['3DSU', '6DS3U', 'D3SU']
SYMB_CTRL = [2238, 3133]
DIST_IAB = [0.5, 1.27, 2.5, 3.56, 4.91]
IAB_PORTS_DL = range(1235, 1235+NUM_IABS)
IAB_PORTS_UL =  range(1435, 1435+NUM_IABS)

CONFIG_PHY = [0,1]

runs = 50
params_grid = {
    'RngRun': list(range(runs)),
    'iabCbPath': f'{CB_PATH_PREFIX_HOME}/8x8.txt',
    'enbCbPath': f'{CB_PATH_PREFIX_HOME}/8x8.txt',
    "useIdealRrc": False,
    "numSymResvForOddIabs": 0,
    "numSymResvCtrl": SYMB_CTRL,
    "simTime": 3,          # [s]
    "ipi": 50,              # [us]
    "appStartMs": 100,      # [ms]
    "dataSensorRate": SRC_RATES,
    "fc": 28e9,             # [Hz]
    "bw": 400e6,             # [Hz]
    "rainRate": RAIN_RATES,   # [mm/h]
    "rlcBufferSize": 25 * 1024 * 1024,
    "slotFormat": SLOT_FORMATS,
    "configurationPhy": CONFIG_PHY,
}

APP_END_SEC = params_grid['simTime']
APP_RUNTIME_SEC = params_grid['simTime'] - params_grid['appStartMs']/1000
APP_START_SEC = params_grid['appStartMs']/1000

OUT_FOLDER = 'Scenario2Plots'

APP_TRACEFILE_UL = 'UL-app-trace-node.txt'
APP_TRACEFILE_DL = 'DL-app-trace-node.txt'
PHY_TRACEFILE = 'RxPacketTrace.txt'


SLOT_DURATION_SEC = 0.125 # [s], considering numerology 3
MARKERS_SF = itertools.cycle(( '^', 'v'))
MARKERS_OH = itertools.cycle(( 's', 'D'))
LINESTYLES_SF = itertools.cycle(( '-', '--'))
LINESTYLES_OH = itertools.cycle(( '-.', ':'))

COLORS = itertools.cycle(('tab:blue', 'tab:red', 'tab:olive', 'tab:purple'))
COLORS_SF = itertools.cycle(('tab:orange', 'tab:cyan'))
COLORS_OH = itertools.cycle(('tab:green', 'tab:pink'))

params_grid_orig = copy.deepcopy(params_grid)
campaign = sem.CampaignManager.load(ns_res_path,
                                    optimized=True, check_repo=False, skip_configuration=True)

if not os.path.isdir(OUT_FOLDER):
    os.makedirs(OUT_FOLDER)

print('Plotting E2E APP layer throughput stats')

for i in range(2):  # 0: DL, 1: UL

    if i==0:
        APP_TRACEFILE = APP_TRACEFILE_DL
        IAB_PORTS = IAB_PORTS_DL
        dir = 'DL'
    else:
        APP_TRACEFILE = APP_TRACEFILE_UL
        IAB_PORTS = IAB_PORTS_UL
        dir = 'UL'

    for configPhy in CONFIG_PHY:
        
        params_grid.update(configurationPhy=configPhy)

        for ueIndex in range(1, 6):

            print ("IAB: ", ueIndex)

            port_iab = IAB_PORTS[ueIndex-1]

            fig, ax = plt.subplots(figsize=[5, 3], dpi=300)

            fig.suptitle(f'Median end-to-end {dir} throughput per IAB node [Mbit/s]')
            fig.tight_layout()
            ax.set_xlabel('Source Rate [Mbit/s]')
            ax.set_ylabel('Throughput [Mbit/s]')
            ax.grid()
            ax.set_title(f'IAB {ueIndex} ({DIST_IAB[ueIndex-1]} km)')

            for symb_ctrl in SYMB_CTRL:

                params_grid.update(numSymResvCtrl = symb_ctrl)

                line = next(LINESTYLES_OH)
                marker = next(MARKERS_OH)

                for i, slot_format in enumerate(SLOT_FORMATS):

                    params_grid.update(slotFormat=slot_format)
                    color = next(COLORS_SF)
                    thr_data = []
                    src_data = []

                    
                    for rate in SRC_RATES:
                        params_grid.update(dataSensorRate=rate)
                        results = campaign.db.get_results(params_grid)
                        thr_e2e_sys, src_rate_sys = np.array(get_e2e_avg_ue_system_throughput(
                            campaign, results, APP_TRACEFILE, APP_RUNTIME_SEC, port_iab))
                        thr_data.append(np.mean(thr_e2e_sys))
                        src_data.append(np.mean(src_rate_sys))

                    ax.plot(src_data, thr_data, label=f'OH: {symb_ctrl}, SF: {SLOT_FORMATS_OUT[i]} ', linestyle=line, color=color, marker=marker)

            label_params = ax.get_legend_handles_labels()

            fig.savefig(f'{OUT_FOLDER}/{configPhy}_THR_{dir}_{ueIndex}IAB.png')

            figl, axl = plt.subplots(dpi=300)
            figl.tight_layout()
            axl.axis(False)
            axl.legend(*label_params, loc="center", bbox_to_anchor=(0.5, 0.5), prop={"size":10})
            figl.savefig(f'{OUT_FOLDER}/{configPhy}_THR_{dir}_legend.png')
            plt.close('all')


print('Plotting PDR')

for i in range(2):  # 0: DL, 1: UL

    if i==0:
        APP_TRACEFILE = APP_TRACEFILE_DL
        IAB_PORTS = IAB_PORTS_DL
        dir = 'DL'
    else:
        APP_TRACEFILE = APP_TRACEFILE_UL
        IAB_PORTS = IAB_PORTS_UL
        dir = 'UL'

    for configPhy in CONFIG_PHY:
        
        params_grid.update(configurationPhy=configPhy)

        for ueIndex in range(1, 6):

            print ("IAB: ", ueIndex)

            port_iab = IAB_PORTS[ueIndex-1]

            fig, ax = plt.subplots(figsize=[5, 3], dpi=300)

            fig.suptitle(f'Average end-to-end {dir} PDR')
            ax.set_xlabel('Source Rate [Mbit/s]')
            ax.set_ylabel(r"Packet Drop Ratio (PDR) $[\%]$")
            ax.grid()
            ax.set_title(f'IAB {ueIndex} ({DIST_IAB[ueIndex-1]} km)')

            for symb_ctrl in SYMB_CTRL:

                params_grid.update(numSymResvCtrl = symb_ctrl)

                line = next(LINESTYLES_OH)
                marker = next(MARKERS_OH)

                for i, slot_format in enumerate(SLOT_FORMATS):

                    params_grid.update(slotFormat=slot_format)
                    color = next(COLORS_SF)
                    pdr_data = []
                    src_data = []

                    
                    for rate in SRC_RATES:
                        params_grid.update(dataSensorRate=rate)
                        results = campaign.db.get_results(params_grid)
                        thr_e2e_sys, src_rate_sys = np.array(get_e2e_avg_ue_system_throughput(
                            campaign, results, APP_TRACEFILE, APP_RUNTIME_SEC, port_iab))
                        avg_thr = np.mean(thr_e2e_sys)
                        src_rate = np.mean(src_rate_sys)
                        pdr = (1-avg_thr/src_rate)*100
                        pdr_data.append(pdr)
                        src_data.append(src_rate)

                    ax.plot(src_data, pdr_data, label=f'OH: {symb_ctrl}, SF: {SLOT_FORMATS_OUT[i]} ', linestyle=line, color=color, marker=marker)

            label_params = ax.get_legend_handles_labels()

            fig.savefig(f'{OUT_FOLDER}/{configPhy}_PDR_{dir}_{ueIndex}IAB.png')

            figl, axl = plt.subplots(dpi=300)
            figl.tight_layout()
            axl.axis(False)
            axl.legend(*label_params, loc="center", bbox_to_anchor=(0.5, 0.5), prop={"size":10})
            figl.savefig(f'{OUT_FOLDER}/{configPhy}_PDR_{dir}_legend.png')
            plt.close('all')

print('Plotting E2E APP layer latency ECDF')

srcRate = SRC_RATES[1]
params_grid.update(dataSensorRate=srcRate, numSymResvCtrl= SYMB_CTRL, slotFormat=SLOT_FORMATS)

for i in range(2):  # 0: DL, 1: UL

    if i==0:
        APP_TRACEFILE = APP_TRACEFILE_DL
        IAB_PORTS = IAB_PORTS_DL
        dir = 'DL'
    else:
        APP_TRACEFILE = APP_TRACEFILE_UL
        IAB_PORTS = IAB_PORTS_UL
        dir = 'UL'

    for configPhy in CONFIG_PHY:
        
        params_grid.update(configurationPhy=configPhy)

        for ueIndex in range(1, 6):

            fig, ax = plt.subplots(figsize=[5, 3], dpi=300)
            fig.suptitle('ECDF end-to-end latency per IAB node [Mbit/s], Source rate: ' + srcRate)
            ax.set_title(f'IAB {ueIndex} ({DIST_IAB[ueIndex-1]} km)')

            print ("IAB: ", ueIndex)
            port_iab = IAB_PORTS[ueIndex-1]
            ax.set_xlabel('Latency [ms]')
            ax.set_ylabel('ECDF')
            ax.grid()

            for symb_ctrl in SYMB_CTRL:

                params_grid.update(numSymResvCtrl = symb_ctrl)

                line = next(LINESTYLES_OH)

                for i, slot_format in enumerate(SLOT_FORMATS):

                    params_grid.update(slotFormat=slot_format)
                    color = next(COLORS_SF)
                    
                    results = campaign.db.get_results(params_grid)
                    lat_samples = np.array(get_e2e_latency_samples_ue(
                    campaign, results, APP_TRACEFILE, port_iab))
                    lat_samples = np.sort (lat_samples)
                    ax.plot(lat_samples, np.linspace(0.0, 1.0, num=len(lat_samples)), label=f'OH: {symb_ctrl}, SF: {SLOT_FORMATS_OUT[i]} ', color = color, linestyle = line)

            
            label_params = ax.get_legend_handles_labels()

            fig.savefig(f'{OUT_FOLDER}/{configPhy}_{dir}_ECDF_{ueIndex}IAB.png')

            figl, axl = plt.subplots(dpi=300)
            figl.tight_layout()
            axl.axis(False)
            axl.legend(*label_params, loc="center", bbox_to_anchor=(0.5, 0.5), prop={"size":10})

            figl.savefig(f'{OUT_FOLDER}/{configPhy}_{dir}_ECDF_Legend.png')
            plt.close('all')
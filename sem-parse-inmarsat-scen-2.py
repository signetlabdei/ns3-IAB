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
ns_res_path = './results-inmarsat-scen-2-closer'
CB_PATH_PREFIX_HOME = os.path.abspath(ns_path) + '/src/mmwave/model/Codebooks'

SRC_RATES = ['25Mbps','50Mbps','75Mbps','100Mbps']
RAIN_RATES = [0, 4, 10, 20]
NUM_IABS = 5

SLOT_FORMATS = ['dddddddsu','dddddsu', 'dddsu']

SLOT_FORMATS_OUT = ['7DSU', '5DSU', '3DSU']
SYMB_CTRL = [2238]
DIST_IAB = [0.5, 1.27, 2.5, 3.56, 4.91]
IAB_PORTS = range(1235, 1235+NUM_IABS)
CONFIG_PHY = [0,1]

runs = 40
params_grid = {
    'RngRun': list(range(runs)),
    'iabCbPath': f'{CB_PATH_PREFIX_HOME}/8x8.txt',
    'enbCbPath': f'{CB_PATH_PREFIX_HOME}/8x8.txt',
    "useIdealRrc": False,
    "numSymResvForOddIabs": 0,
    "numSymResvCtrl": SYMB_CTRL,
    "simTime": 3,          # [s]
    "ipi": 20,              # [us]
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
UDP_APP_TRACEFILE = 'UL-app-trace-node.txt'
PHY_TRACEFILE = 'RxPacketTrace.txt'
IAB_TOPOLOGY_TRACEFILE = 'IAB-topology.txt'
FIRST_IAB_PORT = 1235
# DONOR_CELL_ID = 10
SLOT_DURATION_SEC = 0.125 # [s], considering numerology 3
MARKERS = itertools.cycle(( '*', 'o', 'v'))
LINESTYLES = itertools.cycle(( '-', '-.', '--', ':'))

COLORS = itertools.cycle(('tab:blue', 'tab:red', 'tab:olive', 'tab:purple'))
COLORS_SF = itertools.cycle(('tab:orange', 'tab:cyan', 'tab:brown'))

params_grid_orig = copy.deepcopy(params_grid)
campaign = sem.CampaignManager.load(ns_res_path,
                                    optimized=True, check_repo=False, skip_configuration=True)
if not os.path.isdir(OUT_FOLDER):
    os.makedirs(OUT_FOLDER)

print('Plotting E2E APP layer throughput stats')

for configPhy in CONFIG_PHY:
    
    params_grid.update(configurationPhy=configPhy)

    for ueIndex in range(1, 6):

        print ("IAB: ", ueIndex)

        port_iab = IAB_PORTS_DL[ueIndex-1]

        fig, ax = plt.subplots(figsize=[5, 3], dpi=300)

        fig.suptitle(f'Median end-to-end throughput per IAB node [Mbit/s]')
        fig.tight_layout()
        ax.set_xlabel('Source Rate [Mbit/s]')
        ax.set_ylabel('Throughput [Mbit/s]')
        ax.grid()
        ax.set_title(f'IAB {ueIndex} ({DIST_IAB[ueIndex-1]} km)')

        for rain_rate in RAIN_RATES:
            params_grid.update(rainRate=rain_rate)
            thr_data = []
            src_data = []
            
            for rate in SRC_RATES:
                params_grid.update(dataSensorRate=rate)
                results = campaign.db.get_results(params_grid)
                thr_e2e_sys, src_rate_sys = np.array(get_e2e_avg_ue_system_throughput(
                    campaign, results, UDP_APP_TRACEFILE, APP_RUNTIME_SEC, port_iab))
                thr_data.append(np.mean(thr_e2e_sys))
                src_data.append(np.mean(src_rate_sys))

            ax.plot(SRC_RATES, thr_data, label=f'RR: {rain_rate} mm/h', linestyle=next(LINESTYLES), color=next(COLORS))

        label_params = ax.get_legend_handles_labels()

        fig.savefig(f'{OUT_FOLDER}/{configPhy}_THR_RR_{ueIndex}IAB.png')

        figl, axl = plt.subplots(dpi=300)
        figl.tight_layout()
        axl.axis(False)
        axl.legend(*label_params, loc="center", bbox_to_anchor=(0.5, 0.5), prop={"size":10})
        figl.savefig(f'{OUT_FOLDER}/{configPhy}_THR_RR_legend.png')

        params_grid.update(rainRate=RAIN_RATES)

        fig, ax = plt.subplots(figsize=[5, 3], dpi=300)

        fig.suptitle(f'Median end-to-end throughput per IAB node [Mbit/s]')
        fig.tight_layout()
        ax.set_xlabel('Source Rate [Mbit/s]')
        ax.set_ylabel('Throughput [Mbit/s]')
        ax.grid()
        ax.set_title(f'IAB {ueIndex} ({DIST_IAB[ueIndex-1]} km)')

        for i, slot_format in enumerate(SLOT_FORMATS):

            params_grid.update(slotFormat=slot_format)

            thr_data = []
            src_data = []
            
            for rate in SRC_RATES:
                params_grid.update(dataSensorRate=rate)
                results = campaign.db.get_results(params_grid)
                thr_e2e_sys, src_rate_sys = np.array(get_e2e_avg_ue_system_throughput(
                    campaign, results, UDP_APP_TRACEFILE, APP_RUNTIME_SEC, port_iab))
                thr_data.append(np.mean(thr_e2e_sys))
                src_data.append(np.mean(src_rate_sys))
            
            ax.plot(SRC_RATES, thr_data, label=f'SF: {SLOT_FORMATS_OUT[i]}', color=next(COLORS_SF), marker=next(MARKERS))
        
        label_params = ax.get_legend_handles_labels()

        fig.savefig(f'{OUT_FOLDER}/{configPhy}_THR_SF_{ueIndex}IAB.png')

        figl, axl = plt.subplots(dpi=300)
        figl.tight_layout()
        axl.axis(False)
        axl.legend(*label_params, loc="center", bbox_to_anchor=(0.5, 0.5), prop={"size":10})
        figl.savefig(f'{OUT_FOLDER}/{configPhy}_THR_SF_legend.png')
        plt.close('all')

# plt.show()

print('Plotting E2E APP layer throughput ECDF')

srcRate = SRC_RATES[3]
params_grid.update(dataSensorRate=srcRate)
params_grid.update(slotFormat=SLOT_FORMATS)

for configPhy in CONFIG_PHY:
    
    params_grid.update(configurationPhy=configPhy)

    for ueIndex in range(1, 6):

        fig, ax = plt.subplots(figsize=[5, 3], dpi=300)
        fig.suptitle('ECDF end-to-end throughput per IAB node [Mbit/s], Source rate: ' + srcRate)
        ax.set_title(f'IAB {ueIndex} ({DIST_IAB[ueIndex-1]} km)')

        print ("IAB: ", ueIndex)
        port_iab = IAB_PORTS_DL[ueIndex-1]
        ax.set_xlabel('Throughput [Mbit/s]')
        ax.set_ylabel('ECDF')
        ax.grid()

        for rain_rate in RAIN_RATES:

            params_grid.update(rainRate=rain_rate)
            results = campaign.db.get_results(params_grid)
            thr_e2e_sys, src_rate_sys = np.array(get_e2e_avg_ue_system_throughput(
                    campaign, results, UDP_APP_TRACEFILE, APP_RUNTIME_SEC, port_iab))
                
            thr_e2e_sys = np.sort (thr_e2e_sys)
            thr_e2e_sys = np.append (thr_e2e_sys, 100)

            ax.plot(thr_e2e_sys, np.linspace(0.0, 1.0, num=len(thr_e2e_sys)), label=f'RR: {rain_rate} mm/h', linestyle=next(LINESTYLES), color=next(COLORS))
    
        label_params = ax.get_legend_handles_labels()

        fig.savefig(f'{OUT_FOLDER}/{configPhy}_THR_RR_ECDF_{ueIndex}IAB.png')

        figl, axl = plt.subplots(dpi=300)
        figl.tight_layout()
        axl.axis(False)
        axl.legend(*label_params, loc="center", bbox_to_anchor=(0.5, 0.5), prop={"size":10})
        figl.savefig(f'plot-final-closer/{configPhy}/THR/RR_ECDF_legend.png')

        params_grid.update(rainRate=RAIN_RATES)

        fig, ax = plt.subplots(figsize=[5, 3], dpi=300)

        fig, ax = plt.subplots(figsize=[5, 3], dpi=300)
        fig.suptitle('ECDF end-to-end throughput per IAB node [Mbit/s], Source rate: ' + srcRate)
        ax.set_title(f'IAB {ueIndex} ({DIST_IAB[ueIndex-1]} km)')

        port_iab = IAB_PORTS_DL[ueIndex-1]
        ax.set_xlabel('Throughput [Mbit/s]')
        ax.set_ylabel('ECDF')
        ax.grid()

        for i, slot_format in enumerate(SLOT_FORMATS):

            params_grid.update(slotFormat=slot_format)
            results = campaign.db.get_results(params_grid)
            thr_e2e_sys, src_rate_sys = np.array(get_e2e_avg_ue_system_throughput(
                    campaign, results, UDP_APP_TRACEFILE, APP_RUNTIME_SEC, port_iab))
                
            thr_e2e_sys = np.sort (thr_e2e_sys)
            thr_e2e_sys = np.append (thr_e2e_sys, 100)

            ax.plot(thr_e2e_sys, np.linspace(0.0, 1.0, num=len(thr_e2e_sys)), label=f'SF: {SLOT_FORMATS_OUT[i]}', color=next(COLORS_SF))
            
        label_params = ax.get_legend_handles_labels()

        fig.savefig(f'{OUT_FOLDER}/{configPhy}_THR_SF_ECDF_{ueIndex}IAB.png')

        figl, axl = plt.subplots(dpi=300)
        figl.tight_layout()
        axl.axis(False)
        axl.legend(*label_params, loc="center", bbox_to_anchor=(0.5, 0.5), prop={"size":10})
        figl.savefig(f'plot-final-closer/{configPhy}/THR/SF_ECDF_legend.png')
        plt.close('all')

# plt.show()

print('Plotting E2E APP layer latency ECDF')

srcRate = SRC_RATES[3]
params_grid.update(dataSensorRate=srcRate, slotFormat = SLOT_FORMATS)

for configPhy in CONFIG_PHY:
    
    params_grid.update(configurationPhy=configPhy)

    for ueIndex in range(1, 6):

        fig, ax = plt.subplots(figsize=[5, 3], dpi=300)
        fig.suptitle('ECDF end-to-end latency per IAB node [Mbit/s], Source rate: ' + srcRate)
        ax.set_title(f'IAB {ueIndex} ({DIST_IAB[ueIndex-1]} km)')

        print ("IAB: ", ueIndex)
        port_iab = IAB_PORTS_DL[ueIndex-1]
        ax.set_xlabel('Latency [ms]')
        ax.set_ylabel('ECDF')
        ax.grid()

        for rain_rate in RAIN_RATES:
            params_grid.update(rainRate=rain_rate)

            results = campaign.db.get_results(params_grid)
            lat_samples = np.array(get_e2e_latency_samples_ue(
                campaign, results, UDP_APP_TRACEFILE, port_iab))
            lat_samples = np.sort (lat_samples)
            ax.plot(lat_samples, np.linspace(0.0, 1.0, num=len(lat_samples)), label=f'RR: {rain_rate} mm/h', linestyle=next(LINESTYLES), color=next(COLORS))
        
        label_params = ax.get_legend_handles_labels()

        fig.savefig(f'{OUT_FOLDER}/{configPhy}_LAT_RR_ECDF_{ueIndex}IAB.png')

        figl, axl = plt.subplots(dpi=300)
        figl.tight_layout()
        axl.axis(False)
        axl.legend(*label_params, loc="center", bbox_to_anchor=(0.5, 0.5), prop={"size":10})

        figl.savefig(f'plot-final-closer/{configPhy}/LAT/RR_ECDF_Legend.png')

        plt.close('all')

        params_grid.update(rainRate=RAIN_RATES)

        fig, ax = plt.subplots(figsize=[5, 3], dpi=300)
        fig.suptitle('ECDF end-to-end latency per IAB node [Mbit/s], Source rate: ' + srcRate)
        ax.set_title(f'IAB {ueIndex} ({DIST_IAB[ueIndex-1]} km)')

        ax.set_xlabel('Latency [ms]')
        ax.set_ylabel('ECDF')
        ax.grid()

        for i, slot_format in enumerate(SLOT_FORMATS):
            params_grid.update(slotFormat=slot_format)

            results = campaign.db.get_results(params_grid)
            lat_samples = np.array(get_e2e_latency_samples_ue(
                campaign, results, UDP_APP_TRACEFILE, port_iab))
            lat_samples = np.sort (lat_samples)
            ax.plot(lat_samples, np.linspace(0.0, 1.0, num=len(lat_samples)), label=f'SF: {SLOT_FORMATS_OUT[i]}', color=next(COLORS_SF))
        
        label_params = ax.get_legend_handles_labels()

        fig.savefig(f'{OUT_FOLDER}/{configPhy}_LAT_SF_ECDF_{ueIndex}IAB.png')

        figl, axl = plt.subplots(dpi=300)
        figl.tight_layout()
        axl.axis(False)
        axl.legend(*label_params, loc="center", bbox_to_anchor=(0.5, 0.5), prop={"size":10})

        figl.savefig(f'{OUT_FOLDER}/{configPhy}_LAT_SF_ECDF_Legend.png')

        plt.close('all')

# plt.show()

print('Plotting PDR')

for configPhy in CONFIG_PHY:

    params_grid.update(configurationPhy=configPhy)

    for ueIndex in range(1, 6):

        print ("IAB: ", ueIndex)

        port_iab = IAB_PORTS_DL[ueIndex-1]

        fig, ax = plt.subplots(figsize=[5, 3], dpi=300)

        fig.suptitle(f'Average end-to-end PDR')
        fig.tight_layout()
        ax.set_xlabel('Source Rate [Mbit/s]')
        ax.set_ylabel(r"Packet Drop Ratio (PDR) $[\%]$")
        ax.grid()
        ax.set_title(f'IAB {ueIndex} ({DIST_IAB[ueIndex-1]} km)')

        for rain_rate in RAIN_RATES:
            params_grid.update(rainRate=rain_rate)
            thr_data = []
            src_data = []
            pdr_data = []
            
            for rate in SRC_RATES:
                params_grid.update(dataSensorRate=rate)
                results = campaign.db.get_results(params_grid)
                thr_e2e_sys, src_rate_sys = np.array(get_e2e_avg_ue_system_throughput(
                    campaign, results, UDP_APP_TRACEFILE, APP_RUNTIME_SEC, port_iab))
                avg_thr = np.mean(thr_e2e_sys)
                src_rate = np.mean(src_rate_sys)
                pdr = (1-avg_thr/src_rate)*100
                pdr_data.append(pdr)

            ax.plot(SRC_RATES, pdr_data, label=f'RR: {rain_rate} mm/h', linestyle=next(LINESTYLES), color=next(COLORS))

        label_params = ax.get_legend_handles_labels()

        fig.savefig(f'{OUT_FOLDER}/{configPhy}_THR_RR_PDR_{ueIndex}IAB.png')

        figl, axl = plt.subplots(dpi=300)
        figl.tight_layout()
        axl.axis(False)
        axl.legend(*label_params, loc="center", bbox_to_anchor=(0.5, 0.5), prop={"size":10})
        figl.savefig(f'{OUT_FOLDER}/{configPhy}/THR/RR_PDR_legend.png')
        plt.close('all')

        params_grid.update(rainRate=RAIN_RATES)

        fig, ax = plt.subplots(figsize=[5, 3], dpi=300)

        fig.suptitle(f'Average end-to-end PDR')
        fig.tight_layout()
        ax.set_xlabel('Source Rate [Mbit/s]')
        ax.set_ylabel(r"Packet Drop Ratio (PDR) $[\%]$")
        ax.grid()
        ax.set_title(f'IAB {ueIndex} ({DIST_IAB[ueIndex-1]} km)')

        for i, slot_format in enumerate(SLOT_FORMATS):

            params_grid.update(slotFormat=slot_format)

            thr_data = []
            src_data = []
            pdr_data = []
            
            for rate in SRC_RATES:
                params_grid.update(dataSensorRate=rate)
                results = campaign.db.get_results(params_grid)
                thr_e2e_sys, src_rate_sys = np.array(get_e2e_avg_ue_system_throughput(
                    campaign, results, UDP_APP_TRACEFILE, APP_RUNTIME_SEC, port_iab))
                avg_thr = np.mean(thr_e2e_sys)
                src_rate = np.mean(src_rate_sys)
                pdr = (1-avg_thr/src_rate)*100
                pdr_data.append(pdr)
            
            ax.plot(SRC_RATES, pdr_data, label=f'SF: {SLOT_FORMATS_OUT[i]}', color=next(COLORS_SF), marker=next(MARKERS))
        
        label_params = ax.get_legend_handles_labels()

        fig.savefig(f'{OUT_FOLDER}/{configPhy}_THR_SF_PDR_{ueIndex}IAB.png')

        figl, axl = plt.subplots(dpi=300)
        figl.tight_layout()
        axl.axis(False)
        axl.legend(*label_params, loc="center", bbox_to_anchor=(0.5, 0.5), prop={"size":10})
        figl.savefig(f'{OUT_FOLDER}/{configPhy}_THR_SF_PDR_legend.png')
        plt.close('all')

# plt.show()

print('Plotting PHY occupancy vs Slot Format')

params_grid.update(dataSensorRate=SRC_RATES, rainRate=RAIN_RATES)

for configPhy in CONFIG_PHY:
    
    params_grid.update(configurationPhy=configPhy)

    for rain_rate in RAIN_RATES:

        params_grid.update(rainRate=rain_rate)

        for source_rate in SRC_RATES:

            params_grid.update(dataSensorRate=source_rate)  

            fig, ax = plt.subplots(figsize=[5, 3], dpi=300)

            fig.suptitle(f'Average PHY layer resource occupancy per cell')
            fig.tight_layout()
            ax.set_title(f'RR: {rain_rate} mm/h, Source rate: {source_rate}')
            ax.set_xlabel('SLOT FORMAT')
            ax.set_ylabel(r'PHY occupancy $[\%]$')
            ax.grid()

            phy_occ_list = []

            for sf in SLOT_FORMATS:

                params_grid.update(slotFormat=sf)
                results = campaign.db.get_results(params_grid)

                phy_occ = get_e2e_ue_phy_occupancy(
                    campaign=campaign, results=results, phy_tracename=PHY_TRACEFILE,
                    slot_duration=SLOT_DURATION_SEC, app_start_sec=APP_START_SEC, 
                    sim_duration_dec=APP_END_SEC)

                phy_occ_list.append(phy_occ)

            ax.boxplot(phy_occ_list, labels=SLOT_FORMATS_OUT)
            fig.savefig(f'{OUT_FOLDER}/{configPhy}_PHYOCC_RR_{rain_rate}_SR_{source_rate}_PhyOcc.png')
            plt.close('all')

# plt.show()

print('Plotting SINR stats')

params_grid.update(dataSensorRate=SRC_RATES, slotFormat=SLOT_FORMATS)

for configPhy in CONFIG_PHY:
    
    params_grid.update(configurationPhy=configPhy)

    for ueIndex in range(1,6):

        fig, ax = plt.subplots(figsize=[5, 3], dpi=300)

        fig.suptitle(f'ECDF of the SINR experienced by each packet')
        fig.tight_layout()
        ax.set_xlabel('SINR [dB]')
        ax.set_ylabel('ECDF')
        ax.grid()
        ax.set_title(f'IAB {ueIndex} ({DIST_IAB[ueIndex-1]} km)')

        for rr in RAIN_RATES:

            params_grid.update(rainRate=rr)
            results = campaign.db.get_results(params_grid)
            sinr_samples = np.array(getTxSinrSingleUeSamples(campaign, results, PHY_TRACEFILE,
                                                    ul_or_dl='DL', ueIndex=ueIndex))  # consider DL tx only
            print (f"IAB {ueIndex}, Rain Rate: {rr}, #SINR samples: {len(sinr_samples)}")
            sinr_sorted = np.sort(sinr_samples)
            ax.plot(sinr_sorted, np.linspace(
                0.0, 1.0, num=len(sinr_sorted)), label=f'Rain rate {rr}mm/h')
        
        fig.savefig(f'{OUT_FOLDER}/{configPhy}_SINR_{ueIndex}IAB.png')

        # get handles and labels for reuse
        label_params = ax.get_legend_handles_labels() 

        figl, axl = plt.subplots(dpi=300)
        figl.tight_layout()
        axl.axis(False)
        axl.legend(*label_params, loc="center", bbox_to_anchor=(0.5, 0.5), prop={"size":30})
        #save figure
        figl.savefig(f'plot-final-closer/{configPhy}/SINR/legend.png')
        plt.close('all')

    fig, ax = plt.subplots(figsize=[5, 3], dpi=300)

    fig.suptitle(f'ECDF of the SINR experienced by each packet')
    fig.tight_layout()
    ax.set_xlabel('SINR [dB]')
    ax.set_ylabel('ECDF')
    ax.set_title('Cumulative distribution')

    for rr in RAIN_RATES:

        # Should be basically the same for all rates
        params_grid.update(dataSensorRate=SRC_RATES[0])
        params_grid.update(rainRate=rr)
        results = campaign.db.get_results(params_grid)
        sinr_samples = np.array(getTxSinrSamples(campaign, results, PHY_TRACEFILE,
                                                ul_or_dl='DL'))  # consider UL tx only
        sinr_sorted = np.sort(sinr_samples)
        ax.plot(sinr_sorted, np.linspace(
            0.0, 1.0, num=len(sinr_sorted)), label=f'Rain rate {rr}mm/h')
        
    plt.grid()
    plt.xlim([-20,15])
    fig.savefig(f'{OUT_FOLDER}/{configPhy}_SINR_all.png')

# plt.show()
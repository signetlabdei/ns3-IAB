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
ns_script = 'iab-inmarsat-scenario-1'
ns_res_path = './results-inmarsat-scen-1'
CB_PATH_PREFIX_HOME = os.path.abspath(ns_path) + '/src/mmwave/model/Codebooks'
SRC_RATES = ['50Mbps', '75Mbps', '100Mbps', '125Mbps', '200Mbps']
NUM_IABS = 9

runs = 50
params_grid = {
    'RngRun': list(range(runs)),
    "useIdealRrc": False,
    "numSymResvForOddIabs": 6,
    "simTime": 2,          # [s]
    "ipi": 20,              # [us]
    "appStartMs": 100,      # [ms]
    "dataSensorRate": SRC_RATES,
    "fc": 28e9,             # [Hz]
    "bw": 400e6             # [Hz]
}

APP_END_SEC = params_grid['simTime']
APP_RUNTIME_SEC = params_grid['simTime'] - params_grid['appStartMs']/1000
APP_START_SEC = params_grid['appStartMs']/1000
OUT_FOLDER = 'Scenario1Plots'
UDP_APP_TRACEFILE = 'UL-app-trace-node.txt'
PHY_TRACEFILE = 'RxPacketTrace.txt'
IAB_TOPOLOGY_TRACEFILE = 'IAB-topology.txt'
FIRST_IAB_PORT = 1235
DONOR_CELL_ID = 10
SLOT_DURATION_SEC = 0.25 # [s], considering numerology 2
MARKERS = itertools.cycle((',', '+', '.', 'o', '*'))

params_grid_orig = copy.deepcopy(params_grid)
campaign = sem.CampaignManager.load(ns_res_path,
                                    optimized=True, check_repo=False, skip_configuration=True)
if not os.path.isdir(OUT_FOLDER):
    os.makedirs(OUT_FOLDER)

print('Plotting E2E APP layer throughput stats')
fig = plt.figure(figsize=[5, 3], dpi=300)
thr_data = []
src_data = []
for rate in SRC_RATES:
    params_grid.update(dataSensorRate=rate)
    results = campaign.db.get_results(params_grid)
    thr_e2e_sys, src_rate_sys = np.array(get_e2e_avg_system_throughput(
        campaign, results, UDP_APP_TRACEFILE, APP_RUNTIME_SEC))
    thr_e2e_sys = [point / NUM_IABS for point in thr_e2e_sys]
    src_rate_sys = [point / NUM_IABS for point in src_rate_sys]
    thr_data.append(thr_e2e_sys)
    src_data.append(np.mean(src_rate_sys))

plt.boxplot(thr_data)
plt.plot(np.linspace(1, len(SRC_RATES), num=len(
    SRC_RATES)), src_data, label='Source rate')
plt.title('Median per IAB end-to-end throughput [Mbit/s]')
plt.xlabel('Per IAB source rate [Mbit/s]')
fig.axes[0].set_xticklabels(SRC_RATES)
plt.grid()
plt.legend()
plt.ylabel('Throughput [Mbit/s]')
plt.savefig(f'{OUT_FOLDER}/E2E_per_IAB_throughput_boxplot.png')

print('Plotting E2E APP layer vs IAB depth throughput stats')
fig = plt.figure(figsize=[5, 3], dpi=300)
for rate in SRC_RATES:
    params_grid.update(dataSensorRate=rate)
    results = campaign.db.get_results(params_grid)
    thr_vs_depth_df = get_e2e_avg_system_throughput_vs_depth(
        campaign, results, UDP_APP_TRACEFILE, IAB_TOPOLOGY_TRACEFILE, APP_RUNTIME_SEC, FIRST_IAB_PORT, DONOR_CELL_ID)

    plt.plot(thr_vs_depth_df['depth'], thr_vs_depth_df['metric'], label=f'Source rate {rate}', marker=next(MARKERS))

plt.title('Average end-to-end throughput per IAB node vs depth [Mbit/s]')
plt.xlabel('Source IAB node depth')
fig.axes[0].set_xticks(thr_vs_depth_df['depth'])
plt.grid()
plt.legend()
plt.ylabel('Throughput [Mbit/s]')
plt.savefig(f'{OUT_FOLDER}/E2E_per_IAB_throughput_vs_depth.png')

print('Plotting E2E APP layer vs IAB depth latency stats')
fig = plt.figure(figsize=[5, 3], dpi=300)
for rate in SRC_RATES:
    params_grid.update(dataSensorRate=rate)
    results = campaign.db.get_results(params_grid)
    lat_vs_depth_df = get_e2e_avg_latency_vs_depth(
        campaign, results, UDP_APP_TRACEFILE, IAB_TOPOLOGY_TRACEFILE, APP_RUNTIME_SEC, FIRST_IAB_PORT, DONOR_CELL_ID)

    plt.plot(lat_vs_depth_df['depth'], lat_vs_depth_df['metric'], label=f'Source rate {rate}', marker=next(MARKERS))

plt.title('Average end-to-end latency per IAB node vs depth [ms]')
plt.xlabel('Source IAB node depth')
fig.axes[0].set_xticks(thr_vs_depth_df['depth'])
plt.grid()
plt.legend()
plt.ylabel('Latency [ms]')
plt.savefig(f'{OUT_FOLDER}/E2E_per_IAB_latency_vs_depth.png')

print('Plotting E2E APP layer latency stats')
fig = plt.figure(figsize=[5, 3], dpi=300)
lat_data = []
for rate in SRC_RATES:
    params_grid.update(dataSensorRate=rate)
    results = campaign.db.get_results(params_grid)
    lat_samples = np.array(get_e2e_latency_samples(
        campaign, results, UDP_APP_TRACEFILE))
    lat_data.append(lat_samples)
plt.boxplot(lat_data)
plt.title('Median per packet end-to-end latency [ms]')
plt.xlabel('Per IAB source rate [Mbit/s]')
fig.axes[0].set_xticklabels(SRC_RATES)
plt.grid()
plt.legend()
plt.ylabel('Latency [ms]')
plt.savefig(f'{OUT_FOLDER}/E2E_latency_boxplot.png')

print('Plotting PDR')
fig = plt.figure(figsize=[5, 3], dpi=300)
rates = [50,75,100,125,200] * 10**6
for count, rate in enumerate (SRC_RATES):
    params_grid.update(dataSensorRate=rate)
    results = campaign.db.get_results(params_grid)
    thr_vs_depth_df = get_e2e_avg_system_throughput_vs_depth(campaign, results, UDP_APP_TRACEFILE, IAB_TOPOLOGY_TRACEFILE, APP_RUNTIME_SEC, FIRST_IAB_PORT, DONOR_CELL_ID)
    ber = (1-thr_vs_depth_df['metric']/rates [count])*100
    plt.plot(thr_vs_depth_df['depth'], ber, label=f'Source rate {rate}', marker=next(MARKERS))

plt.title(r"Average end-to-end PDR per IAB node vs depth $[\%]$")
plt.xlabel('Source IAB node depth')
fig.axes[0].set_xticks(thr_vs_depth_df['depth'])
plt.grid()
plt.legend()
plt.ylabel(r"Packet Drop Ratio (PDR) $[\%]$")
plt.savefig(f'{OUT_FOLDER}/E2E_per_IAB_PDR_vs_depth.png')

print('Plotting Latency ECDF')
# Should be basically the same for all rates
fig = plt.figure(figsize=[5, 3], dpi=300)
for rate in SRC_RATES:
    params_grid.update(dataSensorRate=rate)
    results = campaign.db.get_results(params_grid)
    lat_samples = np.array(get_e2e_latency_samples(
        campaign, results, UDP_APP_TRACEFILE))
    lat_samples = lat_samples [range (1,len(lat_samples),5000)]
    
    lat_samples = np.sort (lat_samples)
    lat_samples = np.append (lat_samples, 1600)
    plt.xlim([0,1500])
    plt.plot(lat_samples, np.linspace(
    0.0, 1.0, num=len(lat_samples)), label=rate)

plt.title('ECDF of the Latency experienced by each packet')
plt.xlabel('Latency [ms]')
plt.grid()
plt.legend()
plt.ylabel('ECDF')
plt.savefig(f'{OUT_FOLDER}/latency_ECDF2.png')

print('Plotting PHY occupancy vs depth stats')
# Should be basically the same for all rates
fig = plt.figure(figsize=[5, 3], dpi=300)
for rate in SRC_RATES:
    params_grid.update(dataSensorRate=rate)
    results = campaign.db.get_results(params_grid)
    occupancy_vs_depth_df = getPhyOccupancyVsDepth(
        campaign=campaign, results=results, phy_tracename=PHY_TRACEFILE, 
        topology_tracename=IAB_TOPOLOGY_TRACEFILE, donor_cid=DONOR_CELL_ID, 
        slot_duration=SLOT_DURATION_SEC, app_start_sec=APP_START_SEC, 
        sim_duration_dec=APP_END_SEC)

    plt.plot(occupancy_vs_depth_df['depth'], occupancy_vs_depth_df['metric'], label=f'Source rate {rate}', marker=next(MARKERS))

plt.title('Average PHY layer resource occupancy per cell vs depth')
plt.xlabel('Serving DU node depth')
plt.xticks([0,1,2],[0,1,2])
plt.grid()
plt.legend()
plt.savefig(f'{OUT_FOLDER}/phy_occupancy.png')

print('Plotting SINR stats')
# Should be basically the same for all rates
fig = plt.figure(figsize=[5, 3], dpi=300)
params_grid.update(dataSensorRate=SRC_RATES[0])
results = campaign.db.get_results(params_grid)
sinr_samples = np.array(getTxSinrSamples(campaign, results, PHY_TRACEFILE,
                                         ul_or_dl='UL'))  # consider UL tx only
sinr_sorted = np.sort(sinr_samples)
plt.plot(sinr_sorted, np.linspace(
    0.0, 1.0, num=len(sinr_sorted)), label='SINR [dB]')
plt.title('ECDF of the SINR experienced by each packet')
plt.xlabel('SINR [dB]')
plt.grid()
plt.legend()
plt.ylabel('ECDF')
plt.savefig(f'{OUT_FOLDER}/SINR.png')
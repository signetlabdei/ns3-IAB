import sem
import os
import copy

# Path to the ns-3 installation folder 
ns_path = './'  
# Name of the simulation script
ns_script = 'iab-inmarsat-scenario-1'
# Path where the output of the simulations will be saved
ns_res_path = './results-inmarsat-scen-1'
# Needed for the codebook files. There should not be the need to change this
CB_PATH_PREFIX_HOME = os.path.abspath(ns_path) + '/src/mmwave/model/Codebooks'
# Grid of source rates which will be simulated
dataRates = ['50Mbps', '75Mbps', '100Mbps', '125Mbps', '200Mbps']

# Create the  simulation campaign.
campaign = sem.CampaignManager.new(ns_path, ns_script, ns_res_path, optimized=True, overwrite=False,
                                   runner_type='ParallelRunner', check_repo=False, skip_configuration=True)

# Number of simulation runs for each parameter combination
runs = 50
params_grid = {
    'RngRun': list(range(runs)),
    'iabCbPath': f'{CB_PATH_PREFIX_HOME}/8x8.txt',
    'enbCbPath': f'{CB_PATH_PREFIX_HOME}/8x8.txt',
    # Whether to use an idealized RRC which sends the configuration messages via callbacks, instead of over-the-air
    "useIdealRrc": False,
    # Number of OFDM symbols of each slot which are reserved for odd-depth DUs. See deliverable for additional details
    "numSymResvForOddIabs": 6,
    # Simulation time
    "simTime": 2,          # [s]
    # Inter-packet interval
    "ipi": 20,              # [us]
    # Simulation time at which the applications start. Some waiting time is needed after the simulation start,
    # to wait for the MTs to connect to the DUs
    "appStartMs": 100,      # [ms]
    # Source rate of each IAB node LAN port
    "dataSensorRate": dataRates,
    # Carrier frequency
    "fc": 28e9,             # [Hz]
    # Bandwidth
    "bw": 400e6,            # [Hz]
    # The size of the RLC buffer at the TX side 
    "rlcTxBufferSize": 25 * 1024 * 1024,     # [Bytes]
    # Whether to enable HARQ
    "harqEnabled": True,
    # The TX power of the DUs 
    "duTxPower": 30,        # [dBm]
    # The TX power of the MTs
    "mtTxPower": 30,        # [dBm] 
    # The DUs' noise figure
    "duNoiseFigure": 5.0,   # [dB]
    # The MTs' noise figure
    "mtNoiseFigure": 5.0,   # [dB]
    # Whether to enable the optional shadowing term of the TR 38.901 channel model
    "enableShadowing": False,
    # Whether to enable HARQ
    "harqEnabled": True
}

overall_list = sem.list_param_combinations(params_grid)

# If needed, temporarily limit number of max cores used via:
# sem.parallelrunner.MAX_PARALLEL_PROCESSES = <NUMBER_MAX_THREADS>
campaign.run_missing_simulations(overall_list, stop_on_errors=False)

def check_errors(result):
    result_filenames = campaign.db.get_result_files(result['meta']['id'])
    result_filename = result_filenames['stderr']
    with open(result_filename, 'r') as result_file:
        error_file = result_file.read()
        if (len(error_file) != 0):
            return 1
        else:
            return 0

#params_grid.update(centralizedSched=[False, True])
params_orig = copy.deepcopy(params_grid)
results = campaign.db.get_results()

# Count errors
errors = []
for result_entry in results:
    errors.append(check_errors(result_entry))

num_errors = sum(errors)
print("Overall, we have %d errors out of %d simulations!" % (num_errors, len(results)))
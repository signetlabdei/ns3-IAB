import sem
import os
import copy
import csv

ns_path = './'
ns_script = 'viasat-uniform-grid'
ns_res_path = './results_viasat_uniform_grid'
nRowsCols = ['8,8']
dataRates = ['50Mbps']
rainRates = [0]
slotFormats = ['dddsu']
symbCtrl = [2238]
simTime = [0.2]

# import node positions from csv
# each row will be run as a separate simulation
nodePositions = []
with open("viasat_uniform_grid_xs.csv") as csvfile:
    reader = csv.reader(csvfile)
    nodePositions = list(reader)

nodePositions = [item for sublist in nodePositions for item in sublist]

# Create the  simulation campaigns
campaign = sem.CampaignManager.new(ns_path, ns_script, ns_res_path, optimized=True, overwrite=False,
                                   runner_type='ParallelRunner', check_repo=False, skip_configuration=True)

runs = 1
params_grid = {
    'RngRun': list(range(runs)),
    'nRowsCols': nRowsCols,
    "useIdealRrc": False,
    "numSymResvForOddIabs": 0,
    "numSymResvCtrl": symbCtrl,
    "simTime": simTime,          # [s]
    "ipi": 50,              # [us]
    "appStartMs": 100,      # [ms]
    "dataSensorRate": dataRates,
    "fc": 26e9,             # [Hz]
    "bw": 400e6,             # [Hz]
    "rlcBufferSize": 25 * 1024 * 1024,
    "rainRate": rainRates,   # [mm/h]
    "slotFormat": slotFormats,
    "nodePosition": nodePositions # [m]
}

overall_list = sem.list_param_combinations(params_grid)


# Temporarily limit number of max cores used
#sem.parallelrunner.MAX_PARALLEL_PROCESSES = 19
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

import sem
import os
import copy

ns_path = './'
ns_script = 'iab-inmarsat-scenario-1'
ns_res_path = './results-inmarsat-scen-1'
CB_PATH_PREFIX_HOME = os.path.abspath(ns_path) + '/src/mmwave/model/Codebooks'
dataRates = ['50Mbps', '100Mbps', '200Mbps']

# Create the  simulation campaigns
campaign = sem.CampaignManager.new(ns_path, ns_script, ns_res_path, optimized=True, overwrite=False,
                                   runner_type='ParallelRunner', check_repo=False, skip_configuration=True)

runs = 50
params_grid = {
    'RngRun': list(range(runs)),
    'iabCbPath': f'{CB_PATH_PREFIX_HOME}/8x8.txt',
    'enbCbPath': f'{CB_PATH_PREFIX_HOME}/8x8.txt',
    "useIdealRrc": False,
    "numSymResvForOddIabs": 6,
    "simTime": 2,          # [s]
    "ipi": 20,              # [us]
    "appStartMs": 100,      # [ms]
    "dataSensorRate": dataRates,
    "fc": 28e9,             # [Hz]
    "bw": 400e6             # [Hz]
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
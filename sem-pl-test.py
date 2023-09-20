import sem
import os
import copy
import metrics_common
import numpy as np
from matplotlib import pyplot as plt

ns_path = './'
ns_script = 'inmarsat-pl-test'
ns_res_path = './results-pl-test'
CB_PATH_PREFIX_HOME = os.path.abspath(ns_path) + '/src/mmwave/model/Codebooks'
distances = list(range(100, 5000, 10))

# Create the  simulation campaigns
campaign = sem.CampaignManager.new(ns_path, ns_script, ns_res_path, optimized=True, overwrite=False,
                                   runner_type='ParallelRunner', check_repo=False, skip_configuration=True)

runs = 1
params_grid = {
    "RngRun": 1,
    "cbPath": f'{CB_PATH_PREFIX_HOME}/4x6.txt',
    "dist": distances,           # [m]
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

avg=[]
for dist in distances:
    params_grid={'dist': dist}
    results = campaign.db.get_results(params_grid)
    sinr_samples = np.array(metrics_common.getTxSinrSamples(campaign, results, 'RxPacketTrace.txt'))
    avg.append(np.mean(sinr_samples))

plt.plot(distances, avg)
plt.title('SINR vs distancee\n4x6 URA, isotropic elements @28 GHz')
plt.xlabel('Distance [m]')
plt.ylabel('SINR [dB]')
plt.grid()
plt.savefig('SINR vs distanc')

# ns-3 IAB Simulator: a brief User Guide

## Prerequisites

First of all, the simulator requires all the prerequisites which are needed for the baseline ns-3 simulator to be installed.
This can be achieved by running (assuming an Ubuntu-based Linux distribution):

```bash
sudo apt install g++ python3 python3-dev pkg-config sqlite3 cmake
```

If you wish to run a batch of simulations with multiple parameters (which is usually the case), the [SEM execution manager](https://github.com/signetlabdei/sem), `Numpy` and `Matplotlib` are required as well.
They can be installed via:

```bash
python3 -m pip install sem numpy matplotlib
```

## How to reproduce the Deliverable's simulations

The simulator folder contains the simulation script which was used to obtain the results presented in the deliverable, with the corresponding parsing and plotting utilities.

### Re-running the simulations

To run any of the available scripts, first open a terminal window from the root of the simulator folder and configure ns-3 with:

```bash
./ns3 configure --build-profile=optimized --enable-modules=mmwave
```

and build via:

```bash
./ns3
```

Then, run 

```bash
python3 sem-run-simulations.py
``` 

A progress-bar showing how many simulations are left and an ETA should pop up. 
Please note that it is possible to stop a simulation campaign (CTRL + C) and resume it by launching again the simulation script. However, the simulation runs which were not terminated already will be re-started from scratch.

### Parsing the results

Once the simulations are finished, the results can be parsed by running:

```bash
python3 sem-parse-results.py
``` 

This will parse the simulation results and will generate a predetermined set of plots (along the lines of the ones shown during our periodic meetings) in the folder `Scenario1Plots/`.

## How to run and parse simulations with different parameters

This last section will show how to run the pre-configured simulation scripts with different parameters. The above scripts will be considered as a reference.

### Setting different parameters

Simulations should be run using the execution manager [SEM](https://github.com/signetlabdei/sem). This library manages the parallel execution of the simulations and associates each simulation run to its input parameters. Accordingly, the simulation parameters shall be set in the `sem-run-simulations` script, where the SEM simulation campaign is created. Here, we exposed a set of parameters which can be easily configured.
Parameters common to all simulation runs are set in a Python `Dict` that maps the parameter names to their value. By default, SEM will run `runs` simulations for *every combination* of the provided parameters. For instance, with the following list of parameters:
```python
runs = 10
params = {
  'bw' : [200e6, 400e6],
  'fc'  : [26e9; 60e9],   
}
```
SEM will run *40 simulations in total*, 10 for each combination of the system bandwidth `bw` and carrier frequency `fc`.
Therefore, if you wish to run simulations comparing multiple values of a parameter, set them here only if you wish to run also *all the combinations* of this parameter for *all the combinations* of the other ones.

On the other hand, the number and position of the nodes need to be manually changed in the `iab-inmarsat-scenario-1.cc` simulation script file. Changing these parameters will also require modifying the script which parses the results, as we currently do not support doing this automatically.

### Parsing the results obtained with different parameters

The script `sem-parse-results.py` illustrates how to parse and analyze the simulation results. If you wish to generate the same plots as per-configured, i.e., the same x-axis parameter and y-axis metric, just with different parameters (for instance, additional source rates), it is enough to modify the corresponding parameters in the above scripts.

On the other hand, if you wish to change the x- and/or y-axis parameter/metric, first iterate through the values of the chosen input parameter. Then, compute a metric for each such value. Finally, generate the plot you are interested in using the collection of metric and parameter values. 
For instance, you can iterate through the `bw` values and load the corresponding throughput via:
```python
thr = [] 
  for bw in aListOfBandwidths:
  key = {
    'bw' : bw
  }
  results = campaign.db.get_results(key)
  y = np.array(get_e2e_avg_throughput(campaign, results, \
      APP_TRACENAME, APP_RUN_TIME))
  thr.append(y)
```
Then plot the results according to your needs. You can use the functions defined in `metrics_common.py` to compute other metrics.
Please note that with the above code, *all the results* referring to any simulation run with `bw` = `200e6` are loaded. If you wish to fix any other parameter, specify its value in `key` as a parameter name and value pair.

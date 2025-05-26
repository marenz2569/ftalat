ftalat : Frequency Transition Latency Estimator

ftalat allows you to determine the time taken by your CPU to switch from one frequency to another one.

# Compilation
```
    make
```

# Usage
```
    ./ftalat startFreq targetFreq
    where startFreq is the frequency at the beginning of the test and targetFreq the frequency to switch to
    The program will output the time taken by your CPU to swtich from startFreq to targetFreq
    ftalat must be run with enough permissions to access cpufreq files
```

A script `benchmark.sh` that sets all processor required processor settings and runs ftalat for available frequency combinations is provided.
This script creates a folder `results/$HOSTNAME` that contains all measurement results.

A jupyter notebook `analyze.ipynb` is provided to create plots for each run.
The variable `reference_frequency_per_time_unit` need to be set to the base frequency of the processor in kHz to do the convertion from reference cycles to µs.

# Inner Workings
To measure the transition latency between to frequencies, we benchmark the execution time in reference cyles of a small work loop.
We start with a frequency, switch the frequency to the target frequency and wait until the execution time falls into the expected interquartile range.
The frequency change is then validated by running the loop a few more times and checking if the measured interquartile range overlaps significantly with the expected interquartile range.
This step is repeated to switch back to the start frequency.

## Global variables used in the benchmark
| Variable | Description |
| --- | --- |
| `NB_BENCH_META_REPET` | The number of exections of the loop that is used to build the reference performance. |
| `NB_VALIDATION_REPET` | The number of exections of the loop that is used to validate the performance after a frequency switch. |
| `NB_TRY_REPET_LOOP` | The maximum number of loop executions that we wait for a frequency change. |
| `NB_WAIT_RANDOM` | Flag that sets a random wait delay between 0 and `NB_WAIT_US`. |
| `NB_WAIT_US` | The time to wait between frequency switches. |
| `NB_REPORT_TIMES` | The number of benchmark repetitions. |
| `FREQ_SETTER_FILE` | The file in sysfs that is used to write to frequency. Should be either `scaling_max_freq` or `scaling_setspeed`. |

## Output format
The output of the benchmark is given as tab-seperated values with file starting with `#` as comments.
The output contains following fields:

| Field | Description |
| --- | --- |
| `Change time (with write) [cycles]` | The time the frequency change from the start to the target frequency took with the write to sysfs. |
| `Change time [cycles]` | The time the frequency change from the start to the target frequency took without the write to sysfs. |
| `Write cost [cycles]` | The time the write to sysfs took. |
| `Wait time [us]` | The wait time that was applied after the last frequency change. |
| `Time since last frequency change [cycles]` | The actual number of cycles between the last frequency change the current. |

# Licence
The program is licenced under GPLv3. Please read [COPYRIGHT](https://github.com/marenz2569/ftalat/blob/master/COPYRIGHT) file for more information

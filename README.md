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

# Licence
The program is licenced under GPLv3. Please read [COPYRIGHT](https://github.com/marenz2569/ftalat/blob/master/COPYRIGHT) file for more information

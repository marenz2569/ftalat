/*
 * ftalat - Frequency Transition Latency Estimator
 * Copyright (C) 2013 Universite de Versailles
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE

#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "FreqGetter.h"
#include "FreqSetter.h"

#include "loop.h"
#include "rdtsc.h"
#include "utils.h"

#ifdef _DUMP
#include "dumpResults.h"
#endif

#include "ConfInterval.h"

#define NB_BENCH_META_REPET 10000
#define NB_VALIDATION_REPET 100
#define NB_TRY_REPET_LOOP 1000000

unsigned long times[NB_BENCH_META_REPET];

void usage() {
  fprintf(stdout, "./ftalat [-c coreID] startFreq targetFreq\n");
  fprintf(stdout, "\t-c coreID\t:\tto run the test on a precise core (default 0)\n");
}

inline void wait(unsigned long time_in_us) {
#ifdef NB_WAIT_RANDOM
  time_in_us = xorshf96() % time_in_us;
#endif
  unsigned long long before_time, after_time;
  before_time = getusec();
  do {
    after_time = getusec();
  } while (after_time - before_time < time_in_us);
}

void measureLoop(unsigned int nbMetaRepet) {
  for (unsigned int i = 0; i < nbMetaRepet; i++) {
    times[i] = loop();
#ifdef _DUMP
    writeDump(times[i]);
#endif
  }
}

void runTest(unsigned int startFreq, unsigned int targetFreq, unsigned int coreID) {
  struct ConfidenceInterval TargetInterval, StartInterval;

  {
    setFreq(coreID, targetFreq);
    waitCurFreq(coreID, targetFreq);
    measureLoop(NB_BENCH_META_REPET);
    buildFromMeasurement(times, NB_BENCH_META_REPET, &TargetInterval);
  }

  {
    setFreq(coreID, startFreq);
    waitCurFreq(coreID, startFreq);
    measureLoop(NB_BENCH_META_REPET);
    buildFromMeasurement(times, NB_BENCH_META_REPET, &StartInterval);
  }

  dump(&StartInterval, startFreq, "Start");
  dump(&TargetInterval, targetFreq, "Target");

  // Check if the confidence intervals overlap
  if (StartInterval.LowerBound >= TargetInterval.UpperBound || TargetInterval.LowerBound >= StartInterval.UpperBound) {
    fprintf(stdout, "# Confidence intervals do not overlap, alternatives are "
                    "statistically different with selected confidence level\n");
  } else {
    if ((StartInterval.Average >= TargetInterval.LowerBound && StartInterval.Average <= TargetInterval.UpperBound) ||
        (TargetInterval.Average >= StartInterval.LowerBound && TargetInterval.Average <= StartInterval.UpperBound)) {
      fprintf(stdout, "# Warning: confidence intervals overlap considerably, "
                      "alternatives are equal with selected confidence level\n");
      return;
    } else {
      fprintf(stdout, "# Warning: confidence intervals overlap, we can not "
                      "state any thing, need to do the t-test\n");
    }
  }

  sync();
  loop();
  warmup_cpuid();

  unsigned long measurements[NB_REPORT_TIMES];
  unsigned long measurements_late[NB_REPORT_TIMES];
  unsigned long long measurements_timestamps[NB_REPORT_TIMES];

  for (unsigned int it = 0; it < NB_REPORT_TIMES; it++) {
    char validated = 0;
    unsigned long startLoopTime = 0;
    unsigned long lateStartLoopTime = 0;
    unsigned long endLoopTime = 0;
    unsigned int niters = 0;
    unsigned long time = 0;

#ifdef _DUMP
    resetDump();
#endif

    // Switch frequency to target and wait for the loop timing to be inside the interquartile band
    {
      sync_rdtsc1(startLoopTime);
      setFreq(coreID, targetFreq);
      sync_rdtsc1(lateStartLoopTime);
      do {
        time = loop();
#ifdef _DUMP
        writeDump(time);
#endif
      } while ((time < TargetInterval.Q1 || time > TargetInterval.Q3) && ++niters < NB_TRY_REPET_LOOP);
      sync_rdtsc2(endLoopTime);

      // Validation
      validated = 1;
      measurements[it] = endLoopTime - startLoopTime;
      measurements_late[it] = endLoopTime - lateStartLoopTime;
      measurements_timestamps[it] = endLoopTime;
    }

    // Validate the frequency switch
    {
      struct ConfidenceInterval TargetValidationInterval;

      measureLoop(NB_VALIDATION_REPET);
      buildFromMeasurement(times, NB_VALIDATION_REPET, &TargetValidationInterval);

      // check if TargetValidationInterval and TargetInterval do not overlap
      if (TargetValidationInterval.UpperBound <= TargetInterval.LowerBound ||
          TargetValidationInterval.LowerBound >= TargetInterval.UpperBound) {
        validated = 0;
      }
    }

    // Switch frequency to start and wait for the loop timing to be inside the interquartile band
    {
      setFreq(coreID, startFreq);
      do {
        time = loop();
      } while ((time < StartInterval.Q1 || time > StartInterval.Q3));
    }

    // Validate the frequency switch
    {
      struct ConfidenceInterval StartValidationInterval;

      measureLoop(NB_VALIDATION_REPET);
      buildFromMeasurement(times, NB_VALIDATION_REPET, &StartValidationInterval);

      // check if StartValidationInterval and StartInterval do not overlap
      if (StartValidationInterval.UpperBound <= StartInterval.LowerBound ||
          StartValidationInterval.LowerBound >= StartInterval.UpperBound) {
        validated = 0;
      }
    }

    // Wait some time
    wait(NB_WAIT_US);

    if (validated == 0) {
      measurements[it] = 0;
      measurements_late[it] = 0;
      if (it > 0)
        measurements_timestamps[it] = measurements_timestamps[it - 1];
    }
  }

  fprintf(stdout, "Change time (with write)\tabs. time\tChange time\tWrite cost\n");
  for (unsigned int i = 0; i < NB_REPORT_TIMES; i++) {
    fprintf(stdout, "%lu\t%llu\t%lu\t%lu\n", measurements[i], measurements_timestamps[i] - measurements_timestamps[0],
            measurements_late[i], measurements[i] - measurements_late[i]);
  }
}

void cleanup() {
  closeFreqSetterFiles();

  freeFreqInfo();

#ifdef _DUMP
  closeDump();
#endif
}

int main(int argc, char** argv) {
  if (argc != 3 && argc != 5) {
    usage();
    return -1;
  }

  unsigned int coreID = 0;
  unsigned int startFreq = 0;
  unsigned int targetFreq = 0;

  unsigned int argcCounter = 1;

  // Option for core specification
  if (strcmp(argv[1], "-c") == 0) {
    if (sscanf(argv[2], "%u", &coreID) != 1) {
      fprintf(stderr, "Fail to get the core ID argument\n");
      return -2;
    }
    argcCounter += 2;

    if (argc != 5) {
      fprintf(stderr, "Missing frequencies arguments\n");
      usage();
      return -1;
    }
  }

  if (sscanf(argv[argcCounter], "%u", &startFreq) != 1) {
    fprintf(stderr, "Fail to get the start frequency argument\n");
    return -3;
  }

  if (sscanf(argv[argcCounter + 1], "%u", &targetFreq) != 1) {
    fprintf(stderr, "Fail to get the target freq argument\n");
    return -4;
  }

  // Additional checks
  if (coreID >= getCoreNumber()) {
    fprintf(stdout, "The core ID that user gave is invalid\n");
    fprintf(stdout, "Core ID is set to 0\n");
    coreID = 0;
  }

  initFreqInfo();

#ifdef _DUMP
  openDump("./results.dump", NB_TRY_REPET_LOOP * NB_VALIDATION_REPET);
#endif

  pinCPU(coreID);

  // Set the minimal frequency
  if (openFreqSetterFiles() != 0) {
    cleanup();
    return -3;
  }

  runTest(startFreq, targetFreq, coreID);

  cleanup();

  return 0;
}
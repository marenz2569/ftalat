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

#define NB_BENCH_META_REPET 100000
#define NB_VALIDATION_REPET 100
#define NB_TRY_REPET_LOOP 1000000

unsigned long times[NB_BENCH_META_REPET];

void usage() {
  fprintf(stdout, "./ftalat [-c coreID] startFreq targetFreq\n");
  fprintf(stdout, "\t-c coreID\t:\tto run the test on a precise core (default 0)\n");
}

inline void wait(unsigned long time_in_us) {
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
    // Wait 10ms for settling of the frequency
    wait(10000);
    measureLoop(NB_BENCH_META_REPET);
    buildFromMeasurement(times, NB_BENCH_META_REPET, &TargetInterval);
  }

  unsigned long lastFrequencyChangeRequestCycles = 0;
  unsigned long lastFrequencyChangeCycles = 0;

  {
    sync_rdtsc1(lastFrequencyChangeRequestCycles);
    setFreq(coreID, startFreq);
    waitCurFreq(coreID, startFreq);
    sync_rdtsc2(lastFrequencyChangeCycles);
    // Wait 10ms for settling of the frequency
    wait(10000);
    measureLoop(NB_BENCH_META_REPET);
    buildFromMeasurement(times, NB_BENCH_META_REPET, &StartInterval);
  }

  dump(&StartInterval, startFreq, "Start");
  dump(&TargetInterval, targetFreq, "Target");

  // Check if the confidence intervals overlap
  if (overlapSignificantly(&StartInterval, &TargetInterval)) {
    fprintf(stdout, "# Warning: confidence intervals overlap considerably, "
                    "alternatives are equal with selected confidence level\n");
    return;
  } else if (overlap(&StartInterval, &TargetInterval)) {
    fprintf(stdout, "# Warning: confidence intervals overlap, we can not "
                    "state any thing, need to do the t-test\n");
  } else {
    fprintf(stdout, "# Confidence intervals do not overlap, alternatives are "
                    "statistically different with selected confidence level\n");
  }

  sync();
  loop();
  warmup_cpuid();

  unsigned long measurements[NB_REPORT_TIMES];
  unsigned long measurements_late[NB_REPORT_TIMES];
  unsigned long measurements_timestamp[NB_REPORT_TIMES];
  unsigned long measurements_waitTime[NB_REPORT_TIMES];
  unsigned long measurements_lastFrequencyChangeRequestCycles[NB_REPORT_TIMES];
  unsigned long measurements_lastFrequencyChangeCycles[NB_REPORT_TIMES];

  for (unsigned int it = 0; it < NB_REPORT_TIMES; it++) {
    char validated = 0;
    unsigned long waitTimeUs = 0;

#ifdef _DUMP
    resetDump();
#endif

#ifdef NB_WAIT_RANDOM
    waitTimeUs = xorshf96() % NB_WAIT_US;
#else
    waitTimeUs = NB_WAIT_US;
#endif

    // Wait some time
    wait(waitTimeUs);

    // Switch frequency to target and wait for the loop timing to be inside the interquartile band
    {
      unsigned long startLoopCycles = 0;
      unsigned long lateStartLoopCycles = 0;
      unsigned long endLoopCycles = 0;
      unsigned int niters = 0;
      unsigned long time = 0;

      sync_rdtsc1(startLoopCycles);
      setFreq(coreID, targetFreq);
      sync_rdtsc1(lateStartLoopCycles);
      do {
        time = loop();
#ifdef _DUMP
        writeDump(time);
#endif
      } while ((time < TargetInterval.Q1 || time > TargetInterval.Q3) && ++niters < NB_TRY_REPET_LOOP);
      sync_rdtsc2(endLoopCycles);

      // Validation
      validated = 1;
      measurements[it] = endLoopCycles - startLoopCycles;
      measurements_late[it] = endLoopCycles - lateStartLoopCycles;
      measurements_timestamp[it] = endLoopCycles;
      measurements_waitTime[it] = waitTimeUs;
      measurements_lastFrequencyChangeRequestCycles[it] = startLoopCycles - lastFrequencyChangeRequestCycles;
      measurements_lastFrequencyChangeCycles[it] = endLoopCycles - lastFrequencyChangeCycles;
    }

    // Validate the frequency switch
    {
      struct ConfidenceInterval TargetValidationInterval;

      measureLoop(NB_VALIDATION_REPET);
      buildFromMeasurement(times, NB_VALIDATION_REPET, &TargetValidationInterval);

      if (!overlapSignificantlyQ1Q3(&TargetInterval, &TargetValidationInterval)) {
        validated = 0;
      }
    }

    // Switch frequency to start and wait for the loop timing to be inside the interquartile band
    {
      unsigned long time = 0;

      sync_rdtsc1(lastFrequencyChangeRequestCycles);
      setFreq(coreID, startFreq);
      do {
        time = loop();
      } while ((time < StartInterval.Q1 || time > StartInterval.Q3));
      sync_rdtsc2(lastFrequencyChangeCycles);
    }

    // Validate the frequency switch
    {
      struct ConfidenceInterval StartValidationInterval;

      measureLoop(NB_VALIDATION_REPET);
      buildFromMeasurement(times, NB_VALIDATION_REPET, &StartValidationInterval);

      if (!overlapSignificantlyQ1Q3(&StartInterval, &StartValidationInterval)) {
        validated = 0;
      }
    }

    if (validated == 0) {
      measurements[it] = 0;
      measurements_late[it] = 0;
      measurements_timestamp[it] = 0;
      measurements_waitTime[it] = 0;
      measurements_lastFrequencyChangeRequestCycles[it] = 0;
      measurements_lastFrequencyChangeCycles[it] = 0;
    }
  }

  fprintf(
      stdout,
      "Change time (with write) [cycles]\tChange time [cycles]\tWrite cost [cycles]\tWait time [us]\tTime since last "
      "frequency change request [cycles]\tTime since last frequency change [cycles]\tDetected frequency change "
      "timestamp [cycles]\n");
  for (unsigned int i = 0; i < NB_REPORT_TIMES; i++) {
    fprintf(stdout, "%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\n", measurements[i], measurements_late[i],
            measurements[i] - measurements_late[i], measurements_waitTime[i],
            measurements_lastFrequencyChangeRequestCycles[i], measurements_lastFrequencyChangeCycles[i],
            measurements_timestamp[i]);
  }
}

void cleanup() {
  closeFreqSetterFiles();

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
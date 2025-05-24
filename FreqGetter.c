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

#include <assert.h>
#include <linux/perf_event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>

#include "FreqGetter.h"

unsigned int getCoreNumber() {
  static unsigned int nbCore = 0;

  if (nbCore == 0) {
    // Ask the system for numbers of cores
    nbCore = sysconf(_SC_NPROCESSORS_ONLN);
    if (nbCore < 1) {
      fprintf(stderr, "Fail to get the number of CPU\n");
      nbCore = 1;
    }
  }

  return nbCore;
}

unsigned long long get_cycles(int fd) {
  unsigned long long result;
  size_t res = read(fd, &result, sizeof(unsigned long long));
  if (res != sizeof(unsigned long long))
    return !(0ULL);
  return result;
}

unsigned long long getusec() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (unsigned long long)tv.tv_usec + (unsigned long long)tv.tv_sec * 1000000;
}

void waitCurFreq(unsigned int coreID, unsigned int targetFreq) {
  assert(coreID < getCoreNumber());

  struct perf_event_attr attr;
  int nr = 0;
  static int fd = 0;
  unsigned long long before_cycles, after_cycles, before_time, after_time;
  unsigned int measuredFreq;

  // set up performance counter
  if (fd == 0) {
    memset(&attr, 0, sizeof(struct perf_event_attr));
    attr.type = PERF_TYPE_HARDWARE;
    attr.config = PERF_COUNT_HW_CPU_CYCLES;
    fd = syscall(__NR_perf_event_open, &attr, 0, -1, -1, 0);
  }
  // until target frequency is set
  while (1) {
    before_time = getusec();
    before_cycles = get_cycles(fd);
    // measure 50 us
    do {
      after_time = getusec();
    } while ((after_time - before_time) < 50);

    after_cycles = get_cycles(fd);

    measuredFreq = (after_cycles - before_cycles) * 20;

    // allow 5 % difference
    if (((double)measuredFreq / (double)targetFreq) > 0.95 && ((double)measuredFreq / (double)targetFreq) < 1.05)
      break;
    else if ((nr % 1000) == 900)
      printf("Target: %u, measured: %u\n", targetFreq, measuredFreq);
  }
}
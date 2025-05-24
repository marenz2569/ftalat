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

#ifndef FREQGETTER_H
#define FREQGETTER_H

/**
 * Get the number of online cores
 * \return
 */
unsigned int getCoreNumber();

/**
 * Wait the core identified by \a coreID to be at \a targetFreq frequency
 * \param coreID core identifier
 * \param targetFreq
 */
void waitCurFreq(unsigned int coreID, unsigned int targetFreq);

/**
 * Get current usec in UNIX time
 */
unsigned long long getusec(void);

#endif

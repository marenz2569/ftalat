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

#include "FreqSetter.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <unistd.h>

#include "rdtsc.h"

#include "FreqGetter.h"

#include "utils.h"

FILE** pMinSetFiles = NULL;
FILE** pMaxSetFiles = NULL;

char openFreqSetterFiles()
{
   unsigned int nbCore = getCoreNumber();
      
   pMinSetFiles = malloc(sizeof(FILE*) * nbCore);
   pMaxSetFiles = malloc(sizeof(FILE*) * nbCore);
   
   if (pMinSetFiles == NULL || pMaxSetFiles == NULL)
   {
      fprintf(stdout,"Fail to allocate memory for files\n");
      return -1;
   }
   
   unsigned int i = 0;
   for ( i = 0 ; i < nbCore ; i++ )
   {
      pMinSetFiles[i] = openCPUFreqFile(i,"scaling_min_freq","w");
      if ( pMinSetFiles[i] == NULL )
      {
         return -1;
      }
      pMaxSetFiles[i] = openCPUFreqFile(i,"scaling_max_freq","w");
      if ( pMaxSetFiles[i] == NULL )
      {
         return -1;
      }
   }
   
   return 0;
}

void setFreq(unsigned int coreID, unsigned int targetFreq)
{
   assert(coreID < getCoreNumber());
   
   fprintf(pMinSetFiles[coreID],"%d",targetFreq);
   fflush(pMinSetFiles[coreID]);
   fprintf(pMaxSetFiles[coreID],"%d",targetFreq);
   fflush(pMaxSetFiles[coreID]);
}

void setAllFreq(unsigned int targetFreq)
{
   int nbCore = getCoreNumber();
   int i;
   
   for (i = 0; i < nbCore ; i++ )
   {
      fprintf(pMinSetFiles[i],"%d",targetFreq);
      fflush(pMinSetFiles[i]);
      fprintf(pMaxSetFiles[i],"%d",targetFreq);
      fflush(pMaxSetFiles[i]);

   }
}

void setMinFreqForAll()
{
   setAllFreq(getMinAvailableFreq(0));
   waitCurFreq(0,getMinAvailableFreq(0));
}


void closeFreqSetterFiles(void) 
{
   int nbCore = getCoreNumber();
   int i = 0;
   
   if ( pMinSetFiles )
   {
      for ( i = 0 ; i < nbCore ; i++ )
      {
         if ( pMinSetFiles[i] )
         {
            fclose(pMinSetFiles[i]);
         }
      }
      
      free(pMinSetFiles);
   }

   if ( pMaxSetFiles )
   {
      for ( i = 0 ; i < nbCore ; i++ )
      {
         if ( pMaxSetFiles[i] )
         {
            fclose(pMaxSetFiles[i]);
         }
      }
      
      free(pMaxSetFiles);
   }
}

char setCPUGovernor(const char* newPolicy)
{
   int nbCore = getCoreNumber();
   int i = 0;
   
   assert(newPolicy);
   
   for ( i = 0 ; i < nbCore ; i++ )
   {
      FILE* pGovernorFile = openCPUFreqFile(i,"scaling_governor","w");
      if ( pGovernorFile != NULL )
      {
         fprintf(pGovernorFile,"%s",newPolicy);
      
         fclose(pGovernorFile);
      }
      else
      {
         return -1;
      }
   }
   
   return 0;
}


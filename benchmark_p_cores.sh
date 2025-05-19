#!/bin/sh

mkdir -p 10ksamples/p-core 2> /dev/null

FREQUENCIES=`cat scaling_available_frequencies`

for START in $FREQUENCIES
do
	for TARGET in $FREQUENCIES
	do
		 if [ $START -eq $TARGET ] 
	         then
        	    echo "Skip $START -> $TARGET (same frequency)"
	            continue
        	 fi

		# measurement for long
                export MORE_FLAGS="-DNB_WAIT_RANDOM -DNB_WAIT_US=10000 -DNB_REPORT_TIMES=10000"
                make
                sudo ./ftalat $START $TARGET > 10ksamples/p-core/${START}_${TARGET}-out_random_10000ms_10000sa.txt

	done
done

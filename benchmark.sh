#!/bin/bash

# TODO: remove this once the branch is merged
source ~/lab_management_scripts/.venv/bin/activate
elab ht enable
elab frequency 800
elab cstate enable --only POLL
elab ht disable

# measurement for long
make clean
export MORE_FLAGS="-DNB_WAIT_RANDOM -DNB_WAIT_US=10000 -DNB_REPORT_TIMES=10000"
make

rm -rf results || true
mkdir -p results

FREQUENCIES=(2000000 1900000 1800000 1700000 1600000 1500000 1400000 1300000 1200000 1100000 1000000 900000 800000)

for START in $FREQUENCIES
do
	for TARGET in $FREQUENCIES
	do
		if [ $START -eq $TARGET ] 
	    then
        	echo "Skip $START -> $TARGET (same frequency)"
	    	continue
        fi

		echo "Running $START -> $TARGET"

        sudo ./ftalat $START $TARGET > results/${START}_${TARGET}-out_random_10000us_10000sa.txt
	done
done

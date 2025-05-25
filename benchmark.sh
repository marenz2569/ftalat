#!/usr/bin/env bash

# Take all cpus online
echo on | sudo tee /sys/devices/system/cpu/smt/control

# Test if the processor supports scaling_available_frequencies
# If it does, we will use the userspace governour and write to scaling_setspeed
# if not, we use the performance governor and write to scaling_max_speed
$(test -e /sys/bus/cpu/devices/cpu0/cpufreq/scaling_available_frequencies)
scaling_available_frequencies_found=$?

if [ $scaling_available_frequencies_found -eq 0 ]
then
	frequencies=(`cat /sys/bus/cpu/devices/cpu0/cpufreq/scaling_available_frequencies`)

	echo userspace | sudo tee /sys/bus/cpu/devices/cpu*/cpufreq/scaling_governor
	# Set all threads to the lowest frequency
	cat /sys/bus/cpu/devices/cpu0/cpufreq/scaling_min_freq | sudo tee /sys/bus/cpu/devices/cpu*/cpufreq/scaling_setspeed
else
	# loop over to scaling_min_frequency to scaling_max_frequency in 100kHz steps
	frequencies=()
	min_frequency=`cat /sys/bus/cpu/devices/cpu0/cpufreq/scaling_min_freq`
	max_frequency=`cat /sys/bus/cpu/devices/cpu0/cpufreq/scaling_max_freq`
    for ((i = min_frequency ; i <= max_frequency ; i+=100000)); do
		frequencies+=($i)
	done

	echo performance | sudo tee /sys/bus/cpu/devices/cpu*/cpufreq/scaling_governor
	# Set all threads to the lowest frequency
	cat /sys/bus/cpu/devices/cpu0/cpufreq/scaling_min_freq | sudo tee /sys/bus/cpu/devices/cpu*/cpufreq/scaling_max_freq
fi

echo "Running ftalat for following frequencies:"
printf '%s ' "${frequencies[@]}"
echo ""

# Disable cstates
echo 1 | sudo tee /sys/devices/system/cpu/cpu*/cpuidle/state*/disable

# Take hyperthreads offline
echo off | sudo tee /sys/devices/system/cpu/smt/control

# measurement for long
make clean
if [ $scaling_available_frequencies_found -eq 0 ]
then
	export MORE_FLAGS="-DNB_WAIT_RANDOM -DNB_WAIT_US=10000 -DNB_REPORT_TIMES=10000 -DFREQ_SETTER_FILE=\\\"scaling_setspeed\\\""
else
	export MORE_FLAGS="-DNB_WAIT_RANDOM -DNB_WAIT_US=10000 -DNB_REPORT_TIMES=10000 -DFREQ_SETTER_FILE=\\\"scaling_max_freq\\\""
fi
make

rm -rf results/$HOSTNAME || true
mkdir -p results/$HOSTNAME

for START in "${frequencies[@]}"
do
	for TARGET in "${frequencies[@]}"
	do
		if [ $START -eq $TARGET ] 
		then
			echo "Skip $START -> $TARGET (same frequency)"
			continue
		fi
		echo "Running $START -> $TARGET"
		sudo ./ftalat $START $TARGET > results/$HOSTNAME/${START}_${TARGET}-out_random_10000us_10000sa.txt
	done
done

# Take all cpus online again
echo on | sudo tee /sys/devices/system/cpu/smt/control

# set cpu frequency back to normal
if [ $scaling_available_frequencies_found -ne 0 ]
then
	cat $max_frequency | sudo tee /sys/bus/cpu/devices/cpu*/cpufreq/scaling_max_freq
fi

# Enable cstates
echo 0 | sudo tee /sys/devices/system/cpu/cpu*/cpuidle/state*/disable

# Set green governor
echo powersave | sudo tee /sys/bus/cpu/devices/cpu*/cpufreq/scaling_governor

#!/bin/bash

# Output CSV header
echo "m,n,num_predictions,num_mispredictions,misprediction_rate" > gshare_output.csv

# Path to the trace file
tracefile="../tests/gcc_trace.txt"

# Check if the trace file exists
if [[ ! -f $tracefile ]]; then
    echo "Trace file $tracefile not found!"
    exit 1
fi

# Loop over m from 7 to 20
for (( m=7; m<=20; m++ ))
do
    # Loop over n from 0 to m
    for (( n=0; n<=m; n++ ))
    do
        # Run the simulation and capture the output
        output=$(./sim gshare $m $n $tracefile)

        # Parse the output to extract required data
        num_predictions=$(echo "$output" | grep -i "number of predictions" | awk '{print $NF}')
        num_mispredictions=$(echo "$output" | grep -i "number of mispredictions" | awk '{print $NF}')
        misprediction_rate=$(echo "$output" | grep -i "misprediction rate" | awk '{print $NF}' | tr -d '%')

        # Append the data to the CSV file
        echo "$m,$n,$num_predictions,$num_mispredictions,$misprediction_rate" >> gshare_output.csv

        # Optional: Print progress
        echo "Completed m=$m, n=$n"
    done
done
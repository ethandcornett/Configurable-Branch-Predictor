#!/bin/bash

# Output CSV header
echo "m,trace,num_predictions,num_mispredictions,misprediction_rate" > output.csv

# Loop over m from 7 to 20
for m in {7..20}
do
    # Loop over each trace file
    for trace in gcc jpeg perl
    do
        # Path to the trace file
        tracefile="../tests/${trace}_trace.txt"

        # Check if the trace file exists
        if [[ ! -f $tracefile ]]; then
            echo "Trace file $tracefile not found!"
            continue
        fi

        # Run the simulation and capture the output
        output=$(./sim bimodal $m $tracefile)

        # Parse the output to extract required data
        num_predictions=$(echo "$output" | grep -i "number of predictions" | awk '{print $NF}')
        num_mispredictions=$(echo "$output" | grep -i "number of mispredictions" | awk '{print $NF}')
        misprediction_rate=$(echo "$output" | grep -i "misprediction rate" | awk '{print $NF}' | tr -d '%')

        # Append the data to the CSV file
        echo "$m,$trace,$num_predictions,$num_mispredictions,$misprediction_rate" >> output.csv
    done
done
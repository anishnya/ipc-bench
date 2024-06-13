#!/bin/bash

executable="./fifo"

sizes=(8 64 128 256 512 1024 2048 4096)
rates=(50000 100000 250000 500000 1000000 1500000 250000)


# Loop through each parameter
for size in "${sizes[@]}"; do
    for rate in "${rates[@]}"; do
        count=$((rate * 20))
        # Generate a unique filename based on timestamp and parameter
        filename=$(printf "%s_%s" "$size" "$rate")
        sudo_size=$((size - 8))
        # Run the executable with the current parameter and pipe output to the file
        echo $filename
        cd ~/ipc-bench/build/source/fifo && $executable -c $count -s $sudo_size -r $rate > $filename
        cp $filename ~/ipc-bench/benchmarkOutput/fifo
        cd ~/ipc-bench
    done
done
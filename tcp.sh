#!/bin/bash

executable="./tcp"

sizes=(1 32 256 512 1024 2048 4096)
rates=(1 2 3 4 5 6 7 8 9 10 15 20 25 30 35 40 45 50 55 60 65 70 80 90 100 110 130 150 170)


# Loop through each parameter
for size in "${sizes[@]}"; do
    for rate in "${rates[@]}"; do
        count=$((5 * 1000000))
        # Generate a unique filename based on timestamp and parameter
        filename=$(printf "%s_%s.txt" "$size" "$rate")
        # Run the executable with the current parameter and pipe output to the file
        echo $filename
        cd ~/ipc-bench/build/source/tcp && $executable -c $count -s $size -r $rate > $filename
        cp $filename ~/ipc-bench/benchmarkOutput/tcp
        rm -rf $filename
        cd ~/ipc-bench
    done
done
#!/bin/bash
executable="/home/anishnya/ipc-bench/build/source/domain/domain"

sizes=(128 8192)
rates=(1 2 3 4 5 10 15 30 50 70 90)

# Loop through each parameter
for size in "${sizes[@]}"; do
    for rate in "${rates[@]}"; do
        for i in {1..30}; do
            count=$((100000))
            # Generate a unique filename based on timestamp and parameter
            filename=$(printf "%s_%s_%s.txt" "$size" "$rate" "$i")

            # Timer for 10 seconds on each run
            sleep 5 &
            sleep_pid=$!
            
            $executable -c $count -s $size -r $rate > $filename &
            main_pid=$!
            
            ps -e -o pgid,pid,command | awk -v p=$main_pid '$1 == p {print $2}' >> temp.txt
            file_content=$(cat temp.txt)

            # Wait for the first process to finish using wait
            wait $sleep_pid

            # If the script reaches here, process 1 finished. Kill process 2 (if it's still running)
            counter=0
            array=($file_content)
            sorted_array=($(sort <<< "${array[@]}"))
            
            for element in "${sorted_array[@]:1:3}"; do
                kill -SIGINT "$element"
                
                while ps -p $element > /dev/null; do
                  if [[ $counter -eq 5 ]]; then
                    echo "bad file" $filename
                    kill -9 "$element"
                    break
                  fi
                  
                  sleep 2
                  ((counter++))
                done
            done
            
            echo "Done: " $filename
            cp $filename ~/ipc-bench/benchmarkOutput/socket/new
            rm -rf $filename
            rm -rf temp.txt
        done
    done
done

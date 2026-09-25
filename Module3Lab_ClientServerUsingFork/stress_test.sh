#!/bin/bash

rm -f stress_output.txt

echo "Starting 50 simultaneous clients..."

for i in $(seq 1 50)
do
    ./client >> stress_output.txt 2>&1 &
done

wait

echo "All client processes finished."
echo "Successful responses:"
grep -c "Server response: Hello from server" stress_output.txt

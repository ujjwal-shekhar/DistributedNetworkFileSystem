#!/bin/bash
set -e

echo "Starting OrionFS High-Load Concurrency Stress Test..."

# Cleanup function
cleanup() {
    echo "Cleaning up..."
    docker compose down --volumes --remove-orphans
}
trap cleanup EXIT

# Build the latest image
docker compose build

# Start the cluster
docker compose up -d nm ss-1 ss-2 ss-3 ss-4

echo "Waiting for cluster to stabilize..."
sleep 10

# Stress test parameters
NUM_CLIENTS=10
OPS_PER_CLIENT=5
STRESS_LOG="stress_test.log"
rm -f $STRESS_LOG

echo "Launching $NUM_CLIENTS concurrent clients..."

client_task() {
    local id=$1
    local filename="stress_file_$id.txt"
    docker compose run --rm nm bash -c "./clt nm 8080" <<EOF >> "client_$id.log" 2>&1
CREATE_FILE $filename
WRITE_FILE $filename
Initial content from client $id
END
READ_FILE $filename
WRITE_FILE $filename
Updated content from client $id
END
READ_FILE $filename
exit
EOF
}

# Run clients in parallel
for i in $(seq 1 $NUM_CLIENTS); do
    client_task $i &
done

echo "Waiting for clients to finish..."
wait

echo "Verifying results..."
SUCCESS_COUNT=0
for i in $(seq 1 $NUM_CLIENTS); do
    if grep -q "Updated content from client $i" "client_$i.log"; then
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
    else
        echo "Client $i failed. Check client_$i.log"
    fi
done

echo "Successfully completed $SUCCESS_COUNT / $NUM_CLIENTS concurrent client sessions."

if [ $SUCCESS_COUNT -eq $NUM_CLIENTS ]; then
    echo "OrionFS Concurrency Stress Test Passed!"
else
    echo "OrionFS Concurrency Stress Test Failed!"
    exit 1
fi

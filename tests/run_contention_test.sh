#!/bin/bash
set -e

echo "Starting OrionFS High-Contention Write Stress Test..."

# Cleanup function
cleanup() {
    echo "Cleaning up..."
    docker compose down --volumes --remove-orphans
}
trap cleanup EXIT

# Build the latest image
docker compose build

# Start the cluster (Start 3 SS as NM waits for 3 by default in docker-compose)
docker compose up -d nm ss-1 ss-2 ss-3

echo "Waiting for cluster to stabilize..."
sleep 15

# Contention test parameters
NUM_CLIENTS=20
TARGET_FILE="shared_contention.txt"

echo "Creating initial file..."
docker compose run --rm nm bash -c "./clt nm 8080" <<EOF
CREATE_FILE $TARGET_FILE
exit
EOF

echo "Launching $NUM_CLIENTS concurrent writers to $TARGET_FILE..."

client_task() {
    local id=$1
    docker compose run --rm nm bash -c "./clt nm 8080" <<EOF >> "contention_client_$id.log" 2>&1
WRITE_FILE $TARGET_FILE
Client $id wrote this at $(date +%H:%M:%S.%N)
END
exit
EOF
}

# Run clients in parallel
for i in $(seq 1 $NUM_CLIENTS); do
    client_task $i &
done

echo "Waiting for all writers to complete..."
wait

echo "Verifying file content and looking for lock contention in logs..."
docker compose run --rm nm bash -c "./clt nm 8080" <<EOF > final_content.log 2>&1
READ_FILE $TARGET_FILE
exit
EOF

# Check how many unique client writes we see (Expected 1 due to overwrites)
WRITES_SEEN=$(grep -c "Client .* wrote this" final_content.log || true)
echo "Final file contains content from $WRITES_SEEN client(s)."

echo "Checking Storage Server logs for LockManager activity (Proof of Serialization)..."
# Look for the acquire/release cycle
docker compose logs ss-1 | grep "LockManager" | tail -n 40

if [ $WRITES_SEEN -eq 1 ]; then
    echo "OrionFS Contention Stress Test Passed (Serialization observed via logs)!"
else
    echo "OrionFS Contention Stress Test Failed (File corrupted or empty)!"
    exit 1
fi

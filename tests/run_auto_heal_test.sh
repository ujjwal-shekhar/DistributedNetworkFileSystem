#!/bin/bash
set -e

echo "Starting Auto-Healing & Replication Verification Test..."

# Cleanup function
cleanup() {
    echo "Cleaning up..."
    docker compose down --volumes --remove-orphans
}
trap cleanup EXIT

# Build the latest image
docker compose build

# Start the cluster (NM + 5 SS, target replication 3)
docker compose up -d nm ss-1 ss-2 ss-3 ss-4 ss-5

echo "Waiting for cluster to stabilize..."
sleep 20

echo "1. Creating a file with replication factor 3..."
docker compose run --rm nm bash -c "./clt nm 8080" <<EOF > initial_info.log 2>&1
CREATE_FILE heal_test.txt
WRITE_FILE heal_test.txt
This data must be auto-healed if a node fails.
END
GET_FILE_INFO heal_test.txt
exit
EOF

cat initial_info.log
REPLICA_COUNT=$(grep "Replica count:" initial_info.log | awk '{print $3}')
echo "Initial replica count: $REPLICA_COUNT"

if [ "$REPLICA_COUNT" -ne 3 ]; then
    echo "Error: Initial replica count is not 3. Test FAILED."
    exit 1
fi

# Identify one of the SS IDs to kill
KILL_SS_ID=$(grep "SS ID:" initial_info.log | head -n 1 | awk '{print $NF}' | tr -d ')')
echo "Will kill Storage Server with ID: $KILL_SS_ID"

echo "2. Killing SS-$KILL_SS_ID..."
docker compose kill ss-$KILL_SS_ID
sleep 5

echo "3. Waiting for NM to detect failure and trigger auto-healing..."
# The NM triggers healing when it detects the SS went offline.
# We give it some time to orchestrate the REPLICATE_FILE command.
sleep 15

echo "4. Verifying that replication factor has been restored..."
docker compose run --rm nm bash -c "./clt nm 8080" <<EOF > healed_info.log 2>&1
GET_FILE_INFO heal_test.txt
exit
EOF

cat healed_info.log
NEW_REPLICA_COUNT=$(grep "Replica count:" healed_info.log | awk '{print $3}')
echo "New replica count: $NEW_REPLICA_COUNT"

if [ "$NEW_REPLICA_COUNT" -ne 3 ]; then
    echo "Error: Replica count was not restored to 3. Auto-healing FAILED."
    # Dump NM logs to see what happened
    docker compose logs nm
    exit 1
fi

echo "5. Verifying data integrity..."
docker compose run --rm nm bash -c "./clt nm 8080" <<EOF > final_read.log 2>&1
READ_FILE heal_test.txt
exit
EOF

cat final_read.log
if ! grep -q "This data must be auto-healed" final_read.log || grep -q "v_v/\*" final_read.log; then
    echo "Error: Data corruption or error status detected after healing. Test FAILED."
    exit 1
fi

echo "Auto-Healing Test Completed Successfully!"

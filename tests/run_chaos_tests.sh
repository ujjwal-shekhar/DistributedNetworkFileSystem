#!/bin/bash
set -e

echo "Starting Chaos & Resilience Tests..."

# Cleanup function
cleanup() {
    echo "Cleaning up..."
    docker compose down --volumes --remove-orphans
}
trap cleanup EXIT

# Build the latest image (to ensure it's up to date)
docker compose build

# Start the cluster
docker compose up -d nm ss-1 ss-2 ss-3 ss-4 ss-5

echo "Waiting for cluster to stabilize..."
sleep 20

echo "Initial setup: creating a replicated file..."
docker compose run --rm nm bash -c "./clt nm 8080" <<EOF
CREATE_FILE chaos.txt
WRITE_FILE chaos.txt
Resilience Test Data: This data should survive node failures.
END
exit
EOF

echo "Killing two storage servers (ss-1 and ss-2)..."
docker compose kill ss-1 ss-2
# Give NM a moment to potentially detect (though currently we rely on connection failure during operation)
sleep 2

echo "Verifying file availability with surviving replicas (should succeed)..."
docker compose run --rm nm bash -c "./clt nm 8080" <<EOF
READ_FILE chaos.txt
exit
EOF

echo "Testing creation of a new file with remaining servers..."
docker compose run --rm nm bash -c "./clt nm 8080" <<EOF
CREATE_FILE new_after_chaos.txt
WRITE_FILE new_after_chaos.txt
Data created after partial failure.
END
LIST_FILES
READ_FILE new_after_chaos.txt
exit
EOF

echo "Chaos Tests Completed Successfully."

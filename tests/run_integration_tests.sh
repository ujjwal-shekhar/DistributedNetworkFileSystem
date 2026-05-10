#!/bin/bash
set -e

echo "Starting Integration Tests..."

# Cleanup function
cleanup() {
    echo "Cleaning up..."
    docker compose down --volumes --remove-orphans
}
trap cleanup EXIT

# Build the latest image
docker compose build

# Start the cluster
docker compose up -d nm ss-1 ss-2 ss-3 ss-4 ss-5

echo "Waiting for Naming Server and Storage Servers to stabilize..."
# Give it some time to start and for SS to register
sleep 20

# Run a sequence of commands using the client in a temporary container
echo "Running client commands..."

# Use a Here Document for better readability of client commands
# Capture output to check for failures
if ! docker compose run --rm nm bash -c "./clt nm 8080" <<EOF > test_output.log 2>&1
CREATE_FILE root_file.txt
WRITE_FILE root_file.txt
Content in the root file.
END
CREATE_DIR sub_dir
warp sub_dir
CREATE_FILE nested_file.txt
WRITE_FILE nested_file.txt
This is nested content for redundancy.
END
peek
warp ..
peek
DELETE_DIR sub_dir
peek
exit
EOF
then
    echo "Client container crashed or returned non-zero. Dumping logs..."
    cat test_output.log
    docker compose ps
    docker compose logs
    exit 1
fi

cat test_output.log

if grep -qi "Command Failed" test_output.log || grep -qi "\[ERROR\]" test_output.log || grep -q "v_v/\*" test_output.log; then
    echo "Detected command failure or error status in output. Integration Test FAILED."
    docker compose logs
    exit 1
fi

echo "Integration Tests Completed Successfully."

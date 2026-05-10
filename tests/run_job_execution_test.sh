#!/bin/bash
set -e

echo "Starting Job Execution Verification Test (Docker)..."

# Cleanup function
cleanup() {
    echo "Cleaning up..."
    docker compose down --volumes --remove-orphans
}
trap cleanup EXIT

# Build the latest image
docker compose build

# Start the cluster (NM + 3 SS + 2 JS)
docker compose up -d nm ss-1 ss-2 ss-3 js-1 js-2

echo "Waiting for cluster to stabilize..."
# Need enough time for 3 SS to register so NM starts, and JS to register
sleep 25

echo "1. Creating and writing a test file (unsorted)..."
docker compose run --rm nm bash -c "./clt nm 8080" <<EOF > file_setup.log 2>&1
CREATE_FILE words.txt
WRITE_FILE words.txt
grape honeydew
apple banana cherry
date elderberry fig
END
exit
EOF

cat file_setup.log

echo "2. Running a 'wc' job..."
docker compose run --rm nm bash -c "./clt nm 8080" <<EOF > wc_job.log 2>&1
job wc -w words.txt
exit
EOF

cat wc_job.log
if grep -q "8" wc_job.log; then
    echo "Job 'wc' passed."
else
    echo "Job 'wc' failed. Expected 8 words."
    exit 1
fi

echo "3. Running a 'grep' job..."
docker compose run --rm nm bash -c "./clt nm 8080" <<EOF > grep_job.log 2>&1
job grep "cherry" words.txt
exit
EOF

cat grep_job.log
if grep -q "apple banana cherry" grep_job.log; then
    echo "Job 'grep' passed."
else
    echo "Job 'grep' failed. Line not found."
    exit 1
fi

echo "4. Running a 'sort' job..."
docker compose run --rm nm bash -c "./clt nm 8080" <<EOF > sort_job.log 2>&1
job sort words.txt
exit
EOF

cat sort_job.log
# Verify first line is 'apple banana cherry'
if head -n 10 sort_job.log | grep -q "apple banana cherry"; then
    echo "Job 'sort' passed."
else
    echo "Job 'sort' failed or output order incorrect."
    exit 1
fi

echo "5. Testing job on non-existent file (should fail)..."
docker compose run --rm nm bash -c "./clt nm 8080" <<EOF > fail_job.log 2>&1
job wc missing.txt
exit
EOF

cat fail_job.log
if ! grep -q "v_v/\*" fail_job.log; then
    echo "Failure detection for non-existent file failed."
    exit 1
fi
echo "Error handling passed."

echo "Job Execution Tests Completed Successfully!"

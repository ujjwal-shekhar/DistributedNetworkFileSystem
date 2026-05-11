#!/bin/bash
set -e

echo "Starting C-Shell Features Verification Test (Docker)..."

# Cleanup function
cleanup() {
    echo "Cleaning up..."
    docker compose down --volumes --remove-orphans
}
trap cleanup EXIT

# Build the latest image
docker compose build

# Start the cluster
docker compose up -d nm ss-1 ss-2 ss-3

echo "Waiting for cluster to stabilize..."
sleep 10

echo "1. Testing warp and logical CWD..."
docker compose run --rm nm bash -c "./clt nm 8080" <<EOF > shell_test.log 2>&1
CREATE_DIR dir1
warp dir1
CREATE_FILE file1.txt
WRITE_FILE file1.txt
Content in dir1/file1.txt
END
peek
warp ..
peek
exit
EOF

cat shell_test.log

# Check if file1.txt was seen inside dir1 but not in root (unless explicitly looked for)
if ! grep -q ":/dir1" shell_test.log; then
    echo "Logical CWD 'warp' failed. Prompt did not show /dir1."
    exit 1
fi

echo "2. Testing command delimiters (;, &&, &)..."
docker compose run --rm nm bash -c "./clt nm 8080" <<EOF > delimiter_test.log 2>&1
CREATE_FILE d1; CREATE_FILE d2 && CREATE_FILE d3 & peek
exit
EOF

cat delimiter_test.log
if ! grep -q "d1" delimiter_test.log || ! grep -q "d2" delimiter_test.log || ! grep -q "d3" delimiter_test.log; then
    echo "Command delimiters failed."
    exit 1
fi

echo "3. Testing pastevents (History)..."
docker compose run --rm nm bash -c "./clt nm 8080" <<EOF > history_test.log 2>&1
warp /
peek
pastevents
pastevents purge
pastevents
exit
EOF

cat history_test.log
if ! grep -q "warp /" history_test.log || ! grep -q "peek" history_test.log; then
    echo "Pastevents history tracking failed."
    exit 1
fi

echo "4. Testing status icons (Error/Warning)..."
docker compose run --rm nm bash -c "./clt nm 8080" <<EOF > status_test.log 2>&1
INVALID_COMMAND_BLAH
warp |
exit
EOF

cat status_test.log
if ! grep -q -- "-_-/\*" status_test.log; then
    echo "Warning status indicator (-_-/*) failed."
    exit 1
fi

if ! grep -q -- "v_v/\*" status_test.log; then
    echo "Error status indicator (v_v/*) failed (pipe rejection)."
    exit 1
fi

echo "C-Shell Features Verification Test Completed Successfully!"

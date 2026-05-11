#!/bin/bash

# colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}Starting Phase 3 DAG Simulation...${NC}"

# Cleanup function
cleanup() {
    echo "Cleaning up..."
    docker compose down --volumes --remove-orphans
}
trap cleanup EXIT

# 1. Clean up and build
docker compose down -v
docker compose build

# 2. Start services
docker compose up -d nm ss-1 ss-2 ss-3 js-1 js-2 js-3

echo "Waiting for servers to initialize (15s)..."
sleep 15

# 3. Create a test file in DNFS
echo "Creating test file..."
docker compose run --rm nm bash -c "./clt nm 8080" <<EOF
CREATE_FILE test.txt
WRITE_FILE test.txt
Hello from DNFS Phase 3!
END
exit
EOF

# 4. Run Parallel DAG Simulation
echo -e "${BLUE}Running Parallel DAG: (job sleep 5 & sleep 5 & sleep 5)${NC}"
start_time=$(date +%s)
docker compose run --rm nm bash -c "./clt nm 8080" <<EOF
job sleep 5 & sleep 5 & sleep 5
exit
EOF
end_time=$(date +%s)
duration=$((end_time - start_time))

echo -e "Parallel DAG completed in ${duration} seconds."

if [ $duration -lt 8 ]; then
    echo -e "${GREEN}SUCCESS: Job execution was parallelized (took ${duration}s)!${NC}"
else
    echo -e "${RED}FAILURE: Job execution seems sequential (took ${duration}s).${NC}"
    exit 1
fi

# 5. Run Pipeline DAG Simulation
echo -e "${BLUE}Running Pipeline DAG: job cat test.txt | grep DNFS${NC}"
output=$(docker compose run --rm nm bash -c "./clt nm 8080" <<EOF
job cat test.txt | grep DNFS
exit
EOF
)

echo "$output"

if echo "$output" | grep -q "Hello from DNFS Phase 3!"; then
    echo -e "${GREEN}SUCCESS: Pipeline DAG output verified!${NC}"
else
    echo -e "${RED}FAILURE: Pipeline DAG output incorrect.${NC}"
    exit 1
fi

# 6. Run Complex Mixed DAG Simulation
echo -e "${BLUE}Running Complex DAG: (job cat test.txt | grep Hello & job sleep 2) ; job echo \"DAG Finished\"${NC}"
output=$(docker compose run --rm nm bash -c "./clt nm 8080" <<EOF
(job cat test.txt | grep Hello & job sleep 2) ; job echo "DAG Finished"
exit
EOF
)

echo "$output"

if echo "$output" | grep -q "Hello from DNFS Phase 3!" && echo "$output" | grep -q "DAG Finished"; then
    echo -e "${GREEN}SUCCESS: Complex DAG execution verified!${NC}"
else
    echo -e "${RED}FAILURE: Complex DAG output incorrect.${NC}"
    exit 1
fi

echo -e "${GREEN}Phase 3 DAG Simulation Passed!${NC}"

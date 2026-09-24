#!/bin/bash

# Spostati nella cartella root del progetto rispetto alla posizione dello script
cd "$(dirname "$0")/.."

echo "=== Starting Integration Test ==="

# 1. Compilation
echo "[1/5] Compiling the project..."
make all > /dev/null
if [ $? -ne 0 ]; then
    echo "❌ Compilation failed."
    exit 1
fi

# 2. Quick configuration for testing
echo "[2/5] Preparing test configuration..."
cp conf/config_timeout.conf conf/config_timeout.conf.bak
cp conf/config_explode.conf conf/config_explode.conf.bak

cat <<EOF > conf/config_timeout.conf
SIM_DURATION=2
NOF_WORKERS=3
NOF_WORKERSEATS=4
NOF_USERS=5
NOF_PAUSE=1
NOF_REQUEST=3
P_SERV_MIN=50
P_SERV_MAX=100
EOF

cat <<EOF > conf/config_explode.conf
EXPLODE_THRESHOLD=100
EOF

rm -f stats.csv

# 3. Execution
echo "[3/5] Starting simulation (director)..."
./bin/director > test_output.log 2>&1 &
DIRECTOR_PID=$!

sleep 2

# 4. Dynamic user injection at runtime
echo "[4/5] Injecting dynamic users (add_user)..."
./bin/add_user 3 >> test_output.log 2>&1
if [ $? -ne 0 ]; then
    echo "❌ Failed to inject dynamic users."
fi

echo "Waiting for the simulation to finish (might take a few seconds)..."
wait $DIRECTOR_PID
DIRECTOR_EXIT_CODE=$?

# 5. Result verification
echo "[5/5] Analyzing results..."
TEST_PASSED=1

if [ $DIRECTOR_EXIT_CODE -ne 0 ]; then
    echo "❌ Failed: Program terminated with error code ($DIRECTOR_EXIT_CODE)."
    TEST_PASSED=0
else
    echo "✅ Passed: Director terminated successfully."
fi

if [ -s "stats.csv" ]; then
    echo "✅ Passed: stats.csv file was generated and contains data."
else
    echo "❌ Failed: stats.csv file not found or empty."
    TEST_PASSED=0
fi

# Cleanup
echo "Cleaning up test resources and restoring configurations..."
mv conf/config_timeout.conf.bak conf/config_timeout.conf
mv conf/config_explode.conf.bak conf/config_explode.conf
rm -f test_output.log

if [ $TEST_PASSED -eq 1 ]; then
    echo -e "\n🎉 INTEGRATION TEST PASSED!\nThe multi-process system is stable."
    exit 0
else
    echo -e "\n💀 INTEGRATION TEST FAILED.\nCheck the logs or look for orphaned IPC resources."
    exit 1
fi
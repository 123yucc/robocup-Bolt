#!/bin/bash
# Bolt Team - RoboCup 2D 2026
# RL Performance Test Script
# Tests RL-enhanced agent vs baseline agent

echo "========================================="
echo "Bolt Team RL Performance Test"
echo "RoboCup 2D 2026 - Wuhan University"
echo "========================================="

# Configuration
TEAM_NAME="Bolt"
BASE_DIR="/home/linna/Cyrus2DBase"
BUILD_DIR="${BASE_DIR}/build"
BIN_DIR="${BUILD_DIR}/bin"
LOG_DIR="${BASE_DIR}/logs/rl_test"
NUM_MATCHES=10

# Create directories
mkdir -p ${LOG_DIR}

# Check if binaries exist
if [ ! -f "${BIN_DIR}/sample_player" ]; then
    echo "ERROR: sample_player binary not found!"
    echo "Please build first: cd ${BASE_DIR}/build && make"
    exit 1
fi

# Check if rcssserver is available
if ! command -v rcssserver &> /dev/null; then
    echo "ERROR: rcssserver not found!"
    echo "Please install rcssserver-19.0.x"
    exit 1
fi

echo "Test configuration:"
echo "  - Team name: ${TEAM_NAME}"
echo "  - Number of matches: ${NUM_MATCHES}"
echo "  - Log directory: ${LOG_DIR}"
echo ""

# Results tracking
TOTAL_GOALS_RL=0
TOTAL_GOALS_BASE=0
TOTAL_POSSESSION_RL=0
TOTAL_POSSESSION_BASE=0

echo "Starting test matches..."
echo ""

for i in $(seq 1 ${NUM_MATCHES}); do
    echo "Match ${i}/${NUM_MATCHES}"

    # Create match log directory
    MATCH_LOG="${LOG_DIR}/match_${i}"
    mkdir -p ${MATCH_LOG}

    # Start server
    rcssserver &
    SERVER_PID=$!
    sleep 2

    # Start RL team (left side)
    export BOLT_RL_MODE=1
    cd ${BIN_DIR}
    ./sample_player --team ${TEAM_NAME}_RL &> ${MATCH_LOG}/rl_team.log &
    RL_PID=$!
    sleep 1

    # Start baseline team (right side)
    export BOLT_RL_MODE=0
    ./sample_player --team ${TEAM_NAME}_Base &> ${MATCH_LOG}/base_team.log &
    BASE_PID=$!
    sleep 1

    # Wait for match to complete (6000 cycles = ~10 minutes)
    echo "  Waiting for match to complete..."
    sleep 600

    # Parse results from server log
    # This is a placeholder - real implementation would parse .rcg files
    echo "  Match completed"

    # Kill processes
    kill ${RL_PID} ${BASE_PID} ${SERVER_PID} 2>/dev/null
    sleep 2

    echo ""
done

echo "========================================="
echo "Test completed!"
echo ""
echo "Summary:"
echo "  - Total matches: ${NUM_MATCHES}"
echo "  - Logs saved to: ${LOG_DIR}"
echo ""
echo "To analyze results, run:"
echo "  python ${BASE_DIR}/scripts/training_rl/analyze_performance.py --log ${LOG_DIR}"
echo "========================================="
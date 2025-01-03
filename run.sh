#!/bin/bash

set -e

cleanup() {
    pkill -f "./server" || true
    pkill -f "./client" || true
    exit 0
}

trap cleanup SIGINT SIGTERM EXIT

run_test() {
    local test_name=$1
    local pr=$2
    local pw=$3
    local dt=$4
    local dx=$5

    echo "=== Starting test $test_name ==="
    echo "Parameters: p_r=$pr, p_w=$pw, delta_t=$dt ms, delta_x=$dx sec"
    
    ./server $pr $pw $dt > "server_${test_name}.log" 2>&1 &
    SERVER_PID=$!
    echo "Server started with PID: $SERVER_PID"
    sleep 2

    declare -a CLIENT_PIDS
    for i in {1..5}; do
        ./client $i $dx "competitors_$i.txt" > "client_${i}_${test_name}.log" 2>&1 &
        CLIENT_PIDS[$i]=$!
        echo "Starting client $i with PID: ${CLIENT_PIDS[$i]}"
    done

    for pid in "${CLIENT_PIDS[@]}"; do
        wait $pid || true
    done

    kill $SERVER_PID || true
    wait $SERVER_PID 2>/dev/null || true
    
    sleep 2
    echo "=== Test $test_name completed ==="
}

main() {
    mkdir -p build
    cd build
    cmake .. && make -j$(nproc)
    
    python3 ../data_generator.py 5 100
    cp ../competitors_*.txt .

    declare -A tests=(
        ["A1"]="4 4 1 1"
        ["A2"]="4 4 2 1"
        ["A3"]="4 4 4 1"
        ["B1"]="2 2 1 1"
        ["B2"]="2 2 2 1"
        ["B3"]="2 2 4 1"
        ["C1"]="4 2 1 1"
        ["C2"]="4 2 2 1"
        ["C3"]="4 2 4 1"
        ["D1"]="4 8 1 1"
        ["D2"]="4 8 2 1"
        ["D3"]="4 8 4 1"
    )

    for test_name in "${!tests[@]}"; do
        read -r pr pw dt dx <<< "${tests[$test_name]}"
        run_test "$test_name" "$pr" "$pw" "$dt" "$dx"
    done

    echo "All tests completed"
}

main "$@"
#!/bin/bash

set -e

cleanup() {
    echo "Cleaning up..."
    pkill -f "./server" || true
    pkill -f "./client" || true
    rm -f *.txt *.log
    sleep 2
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
    
    python3 data_generator.py 5 100
    
    ./server $pr $pw $dt > "server_${test_name}.log" &
    SERVER_PID=$!
    echo "Server started with PID: $SERVER_PID"
    sleep 5

    declare -a CLIENT_PIDS
    for i in {1..5}; do
        ./client $i $dx "competitors_$i.txt" > "client_${i}_${test_name}.log" &
        CLIENT_PIDS[$i]=$!
        echo "Client $i started with PID: ${CLIENT_PIDS[$i]}"
        sleep 1
    done

    for pid in "${CLIENT_PIDS[@]}"; do
        wait $pid || true
    done

    kill -TERM $SERVER_PID || true
    wait $SERVER_PID 2>/dev/null || true
    
    cleanup
    echo "=== Test $test_name completed ==="
    sleep 5
}

main() {
    rm -rf build
    mkdir -p build
    cd build
    cmake .. && make -j$(nproc)
    cp ../data_generator.py .

    declare -A tests=(
        ["A1"]="4 4 1 1"
    )

    for test_name in "${!tests[@]}"; do
        read -r pr pw dt dx <<< "${tests[$test_name]}"
        run_test "$test_name" "$pr" "$pw" "$dt" "$dx"
    done
}

main "$@"
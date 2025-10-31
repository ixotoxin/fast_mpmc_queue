#!/bin/bash

BINS=(test_mpsc test_mpmc test_fast_mpsc test_fast_mpmc mpmc_stress_test)
[ ! -e ./log ] && mkdir ./log
for FILE in "${BINS[@]}"; do
    BIN_FILE=./bin/${FILE}
    LOG_FILE=./log/${FILE}_output.log
    [ -e "$BIN_FILE" ] && "$BIN_FILE" > "$LOG_FILE" 2>&1 && "$BIN_FILE" > "$LOG_FILE" 2>&1
done

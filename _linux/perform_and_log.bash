#!/bin/bash

BINS=(test_mpscq test_mpmcq test_mpmcqdd test_mpmcqsl test_dfmpscq test_dfmpmcq test_sfmpscq test_sfmpmcq stress_test_mpmc)
[ ! -e ./log ] && mkdir ./log
for FILE in "${BINS[@]}"; do
    BIN_FILE=./bin/${FILE}
    LOG_FILE=./log/${FILE}_output.log
    [ -e "$BIN_FILE" ] && "$BIN_FILE" > "$LOG_FILE" 2>&1 && "$BIN_FILE" > "$LOG_FILE" 2>&1
done

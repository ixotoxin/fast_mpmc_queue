#!/bin/bash

BINS=(test_mpsc test_mpmc test_mpmcdd test_mpmcsl test_dfmpmcq test_dfmpscq test_sfmpmcq test_sfmpscq)
[ ! -e ./log ] && mkdir ./log
for FILE in "${BINS[@]}"; do
    BIN_FILE=./bin/${FILE}
    LOG_FILE=./log/${FILE}_output.log
    [ -e "$BIN_FILE" ] && "$BIN_FILE" > "$LOG_FILE" 2>&1 && "$BIN_FILE" > "$LOG_FILE" 2>&1
done

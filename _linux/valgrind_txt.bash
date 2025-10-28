#!/bin/bash

VG_OPTS="--tool=memcheck --leak-check=full --leak-resolution=high --show-leak-kinds=all --show-error-list=yes"
VG_OPTS="$VG_OPTS --keep-debuginfo=yes --vgdb=no --track-origins=yes --num-callers=100"
BINS=(test_mpsc test_mpmc test_fast_mpsc test_fast_mpmc)
[ ! -e ./log ] && mkdir ./log
for FILE in "${BINS[@]}"; do
    BIN_FILE=./bin/${FILE}
    LOG_FILE=./log/${FILE}_valgrind.log
    [ -e "$BIN_FILE" ] && valgrind $VG_OPTS --log-file="$LOG_FILE" "$BIN_FILE"
done

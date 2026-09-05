#!/bin/bash
trap "echo Error!" ERR
ARGS=("$@")

echo "First arg: ${ARGS[0]}"

for arg in "$@"; do
    echo "$arg"
done

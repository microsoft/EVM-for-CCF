#!/bin/bash
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
if [ -z "$1" ]
  then
    echo "No file given"
    exit 1
fi

if [ ! -f "$1" ]
  then
    echo "File $1 not found"
    exit 2
fi

BASENAME=${1%.sol}
COMBINED_PATH=${BASENAME}_combined.json

# $ solc --version
# solc, the solidity compiler commandline interface
# Version: 0.5.10+commit.5a6ea5b1.Linux.g++

echo "Writing to ${COMBINED_PATH}"

solc --combined-json abi,bin,bin-runtime,hashes --evm-version homestead --pretty-json --optimize "$1"> "${COMBINED_PATH}"

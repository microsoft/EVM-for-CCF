#!/bin/bash
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set -e

if [ ! -f "env/bin/activate" ]
    then
        python3.7 -m venv env
fi

source env/bin/activate
pip install wheel
pip install -U -r ../CCF/tests/requirements.txt
pip install -U -r ../tests/requirements.txt

ctest "$@"

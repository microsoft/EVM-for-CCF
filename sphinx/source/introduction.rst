Introduction
============

Overview
--------

EVM for CCF is a sample CCF application. It runs EVM bytecode transactions through `Enclave EVM (eEVM) <https://github.com/Microsoft/eEVM/>`_, with all permanent state backed by CCF's replicated, fault-tolerant :cpp:class:`kv::Store`. Since  transactions are executed inside the enclave this can provide confidential, attestable execution of EVM bytecode and smart contracts at significantly higher throughputs (and lower latencies) than most Ethereum networks.

The protocol is based on the `Ethereum JSON RPC <https://github.com/ethereum/wiki/wiki/JSON-RPC>`_ specification. It is not fully compliant - many methods are not implemented, some response fields are left blank, and additional methods are available for setup and debugging.

Building and running tests
--------------------------

To build from scratch:

.. code-block:: bash

    mkdir build
    cd build
    cmake -GNinja ..
    ninja

To run tests:

.. code-block:: bash

    cd build
    ./tests.sh -VV



Some of the tests spin up a CCF network using the Python infrastructure from the CCF repo. This requires several Python libraries to be installed including `web3.py <https://github.com/ethereum/web3.py>`_.
The ``tests.sh`` wrapper takes care of that.

This is not currently tested on wide variety of platforms. We develop on Ubuntu18.04, with Python3.7, compiling with Clang7 - there may be compatibility problems with other versions.

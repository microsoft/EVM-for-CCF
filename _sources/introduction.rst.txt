Introduction
============

Overview
--------

`EVM for CCF`_ is a sample application for the `Confidential Consortium Framework (CCF) <https://github.com/Microsoft/CCF>`_. It runs EVM bytecode transactions through `Enclave EVM (eEVM) <https://github.com/Microsoft/eEVM/>`_, with all permanent state backed by CCF's replicated, fault-tolerant :cpp:class:`kv::Store`. Since transactions are executed inside the enclave this can provide confidential, attestable execution of EVM bytecode and smart contracts at significantly higher throughputs (and lower latencies) than most Ethereum networks.

An EVM for CCF service is a distributed network of TEEs running this sample application. This service can look like a standard Ethereum service, exposing the same API methods based on the `Ethereum JSON RPC <https://github.com/ethereum/wiki/wiki/JSON-RPC>`_ specification, but it has several crucial differences:

* Users trust responses because they come from an auditable TEE, rather than because they come from a local node. Users do not run their own local nodes repeating every transaction execution - they contact a trusted service.
* Consensus is deterministic rather than probabilistic. Rather than waiting for multiple proof-of-work blocks, users know that their results have been persisted once CCF replicates them (via Raft or PBFT).
* Private state can be kept in smart contracts. Contract state is not publically visible on the ledger, it is only visible to the executing nodes. So contracts can work directly with private state, rather than hashes or transforms of it.

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

Some of the tests spin up a CCF network using the Python infrastructure from the CCF repo. This requires several Python libraries to be installed, including `web3.py <https://github.com/ethereum/web3.py>`_.
The ``tests.sh`` creates a temporary Python environment and installs these dependencies before running the tests.

This project is not currently tested on wide variety of platforms. We develop on Ubuntu18.04 with Python3.7, compiling with Clang8 - there may be compatibility problems with other versions.

.. _`EVM for CCF`: https://github.com/microsoft/EVM-for-CCF
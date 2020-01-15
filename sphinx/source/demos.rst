Demos
=====

A client needs to send `JSON-RPC <https://www.jsonrpc.org/specification>`_ requests over `TLS <https://tools.ietf.org/html/rfc5246>`_, as with other CCF apps.

This repo contains 3 demo clients. All demonstrate deployment of an `ERC20 <https://theethereum.wiki/w/index.php/ERC20_Token_Standard>`_ contract, followed by transactions which transfer tokens between multiple accounts. The Solidity source of the contract is in ``samples/contracts/ERC20.sol``, and the precompiled result ``ERC20_combined.sol`` is parsed by the tests.

The commands to run each of these demos (and the environment variables that may need to be set) can be seen by running:

.. code-block:: bash

    ctest -VV -N

Python
------

The source for this demo is in ``samples/erc20/erc20_deploy_and_transfer.py``. It creates a new CCF network consisting of 2 local nodes, leverages `web3.py <https://github.com/ethereum/web3.py>`_ to parse the contract constructor and function ABI from json, deploys an instance of the parsed ERC20 contract, and finally transfers tokens between ethereum addresses, verifying from the event logs that the correct transfer occurred.

Some RPC methods are wrapped as functions, for example ``eth_sendTransaction``:

.. literalinclude:: ../../samples/erc20/contracts.py
    :pyobject: send_transaction

and ``eth_getTransactionReceipt``:

.. literalinclude:: ../../samples/erc20/contracts.py
    :pyobject: get_transaction_receipt

Where RPC responses contain ethereum addresses they are encoded as upper-case hex-strings rather than normalised or checksum formats - ``to_checksum_address`` is used to produce canonical, comparable strings.

C++
---

The source for this demo is in ``src/clients/cpp/erc20_client.cpp``. In the full end-to-end test, this is called by ``samples/erc20/run_erc20_cpp.py`` after the Python infrastructure has created a network.

Much of the code for this demo deals with parsing and ABI encoding function parameters to produce callable EVM bytecode. The types and functions in ``include/abi_encoding.h`` and ``include/contract.h`` should be reusable for other contracts, though they do not yet handle every possible Ethereum argument type.

The body of the test does similar work to the Python client - parsing the ERC20 contract data from json, deploying an instance of the contract, interacting with the contract and verifying the results.

JSON Scenario
-------------

This demo builds on CCF's generic `end-to-end JSON scenario <https://ccfdoc.z6.web.core.windows.net/demo.html>`_ test - read that for an overview of the general scenario format.

This scenario is specified by ``samples/erc20/erc20_transfers_scenario.json``. The transactions are similar to those in the above tests. First it calls ``eth_sendTransaction`` to deploy an ERC20 contract, then ``eth_sendTransaction`` to transfer tokens and ``eth_call`` to read token balances. One key difference is that this uses fixed addresses rather than generating fresh random addresses each run - this lets us statically predict the deployed contract address.

The simplest way to test running your own contracts and transactions over CCF is to edit this scenario file, or pass your own scenario file on the command line.


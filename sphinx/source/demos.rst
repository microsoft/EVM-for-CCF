Demos
=====

A client currently needs to send `JSON-RPC <https://www.jsonrpc.org/specification>`_ requests over `TLS <https://tools.ietf.org/html/rfc5246>`_, as with other CCF apps (HTTP and WebSockets support is coming soon).

The following demos show deployment of an `ERC20 <https://theethereum.wiki/w/index.php/ERC20_Token_Standard>`_ contract, followed by transactions which transfer tokens between multiple accounts. The Solidity source of the contract is in ``tests/contracts/ERC20.sol``, and the precompiled result ``ERC20_combined.sol`` is parsed by the demos.

The commands to run each of these demos (and the environment variables that may need to be set) can be seen by running:

.. code-block:: bash

    ctest -VV -N

Python
------

The source for this demo is in ``samples/erc20_deploy_and_transfer.py``. It uses a `web3.py <https://github.com/ethereum/web3.py>`_ with a custom `provider` to communicate with CCF. The demo deploys a new CCF network consisting of 2 local nodes, deploys an instance of the parsed ERC20 contract, and then transfers tokens between ethereum addresses.

Note that the ``Caller`` defined in ``samples/utils.py`` is used to work around some assumptions in ``web3.py``. For instance the ``gas`` and ``gasPrice`` fields must be filled in explicitly, or else ``web3`` will make unsupported RPC calls to find sensible values. It is possible to use this provider to interact with EVM-for-CCF from a live terminal, but care must be taken to avoid these unsupported RPCs.

JSON Scenario
-------------

This demo builds on CCF's generic `end-to-end JSON scenario <https://microsoft.github.io/CCF/developers/demo.html>`_ test - read that for an overview of the general scenario format.

This scenario is specified by ``tests/erc20_transfers_scenario.json``. The transactions are similar to those above. First it calls ``eth_sendTransaction`` to deploy an ERC20 contract, then ``eth_sendTransaction`` to transfer tokens and ``eth_call`` to read token balances. One key difference is that this uses fixed addresses rather than generating fresh addresses (from new Ethereum identities) each run - this lets us statically predict the deployed contract address.

The simplest way to test running your own contracts and transactions against EVM for CCF is to edit this scenario file, or pass your own scenario file on the command line.


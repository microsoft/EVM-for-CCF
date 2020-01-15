Implementation
==============

The basic structure of a CCF app can be seen in the `example logging app <https://ccfdoc.z6.web.core.windows.net/example.html>`_ in the CCF repo. This app implements :cpp:func:`ccfapp::get_rpc_handler` in a similar way:

.. literalinclude:: ../../src/app/ethereum.cpp
    :language: cpp
    :start-after: SNIPPET_START: rpc_handler
    :end-before: SNIPPET_END: rpc_handler
    :dedent: 2

Multiple tables are created in CCF's :cpp:type:`kv::Store`:

* a map from ethereum addresses to :cpp:type:`eevm::Account` (containing the current ether balance and the account's executable code, if any)
* a map for all the EVM-accessible per-account storage (keyed by a concatenation of the account address and storage key)
* a map from transaction hashes to their results, sufficient for producing minimal transaction receipts

.. literalinclude:: ../../src/app/ethereum.cpp
    :language: cpp
    :start-after: SNIPPET_START: initialization
    :end-before: SNIPPET_END: initialization
    :dedent: 2

Interface between EVM execution and KV-storage
----------------------------------------------

The constructor installs handlers for each supported RPC method. The body of each these handlers gets a set of :cpp:class:`kv::Map::TxView` over the required tables, and uses these to create an instance of :cpp:type:`evm4ccf::EthereumState`. This is an implementation of the abstract :cpp:type:`eevm::GlobalState` which reads and writes from those `TxViews`, backed by CCF's :cpp:class:`kv::Store`.

Where the transaction results in the execution of (ie - `eth_sendTransaction`, `eth_call`), the handler passes this :cpp:type:`evm4ccf::EthereumState` to :cpp:func:`evm4ccf::EthereumFrontend::run_in_evm` which in turn calls :cpp:func:`eevm::Processor::run`. Any SSTORE opcodes in the executed bytecode will then result in writes to the EthereumState's :cpp:class:`kv::Map::TxView` :

.. literalinclude:: ../../src/app/account_proxy.h
    :language: cpp
    :start-after: SNIPPET_START: store_impl
    :end-before: SNIPPET_END: store_impl
    :dedent: 2

This adds to the CCF-transaction's write set, updating an element in the `Storage` table to the provided value. If this transaction is successfully committed, that write set and corresponding storage update will be replicated across all CCF nodes.

Notes
-----

- eEVM does not track gas. Gas is not spent during computation, out-of-gas exceptions will not occur.
- Previous block state is inaccessible. EVM bytecode can access blockchain state directly through opcodes like BLOCKHASH, NUMBER, TIMESTAMP. These will return placeholders in the CCF-Eth app, since there is no historical storage to read from.
- The ``dbg_create`` RPC may be used to provision an account with specified storage, code, and balance. This is a useful development feature but should be removed for any production builds.
- Standard Ethereum "transaction hashes" are the hash of the `signed` transaction, but we return the hash of the unsigned transaction.
- Live networks operate on the "raw" transaction, deriving the sender's address through ECDSA recovery. This app does not handle raw transactions or ECDSA recovery - any caller with permission to execute transactions can do so `from any address`.
- Event logs are stored in the KV resulting in rapid expansion of memory footprint, swapping outside of the SGX EPC memory, and poor performance.
- Transaction receipts are largely empty. Only the fields needed for current demos are filled in.

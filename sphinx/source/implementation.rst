Implementation
==============

The basic structure of a CCF app can be seen in the `example logging app <https://microsoft.github.io/CCF/developers/logging_cpp.html>`_ in the CCF repo. This app implements :cpp:func:`ccfapp::get_rpc_handler` in a similar way:

.. literalinclude:: ../../src/app/evm_for_ccf.cpp
    :language: cpp
    :start-after: SNIPPET_START: rpc_handler
    :end-before: SNIPPET_END: rpc_handler
    :dedent: 2

Multiple tables are created in CCF's :cpp:type:`kv::Store`:

* maps from ethereum addresses to each piece of account state (ether balance, executable code, transaction nonce)
* a map for all the EVM-accessible per-account storage (keyed by a concatenation of the account address and storage key)
* a map from transaction hashes to their results, sufficient for producing minimal transaction receipts

.. literalinclude:: ../../src/app/evm_for_ccf.cpp
    :language: cpp
    :start-after: SNIPPET_START: initialization
    :end-before: SNIPPET_END: initialization
    :dedent: 2

Interface between EVM execution and KV-storage
----------------------------------------------

The constructor installs handlers for each supported RPC method. The body of each these handlers constructs an instance of :cpp:type:`evm4ccf::EthereumState` with :cpp:class:`kv::Map::TxViews` over the required tables. This is an implementation of the abstract :cpp:type:`eevm::GlobalState` which reads and writes from those :cpp:class`TxViews`, backed by CCF's :cpp:class:`kv::Store`.

Where the transaction results in the execution of EVM bytecode (ie - `eth_sendTransaction`, `eth_call`), the handler passes this :cpp:type:`evm4ccf::EthereumState` to :cpp:func:`evm4ccf::EthereumFrontend::run_in_evm` which in turn calls :cpp:func:`eevm::Processor::run`. Any ``SSTORE`` opcodes in the executed bytecode will then result in writes to the EthereumState's :cpp:class:`kv::Map::TxView` :

.. literalinclude:: ../../src/app/account_proxy.h
    :language: cpp
    :start-after: SNIPPET_START: store_impl
    :end-before: SNIPPET_END: store_impl
    :dedent: 2

This adds to the CCF-transaction's write set, updating an element in the ``Storage`` table to the provided value. If this transaction is successfully committed, that write set and corresponding storage update will be replicated across all CCF nodes.

Notes
-----

- eEVM does not track gas. Gas is not spent during computation, and out-of-gas exceptions will not occur.
- eEVM does not support post-Homestead opcodes or precompiled contracts. When compiling to EVM bytecode, be sure to specify the target EVM version.
- Previous block state is inaccessible. EVM bytecode can access blockchain state directly through opcodes like ``BLOCKHASH``, ``NUMBER``, ``TIMESTAMP``. These will return placeholders in EVM for CCF, since there is no historical storage to read from.
- While ``eth_getTransactionReceipt`` is implemented, and can be used to retrieve the address of deployed contracts, many of the receipt fields are not applicable to EVM for CCF and are left empty.
- Event logs are not stored. These could be stored in the KV as well, with some decisions made regarding their intended privacy level. But we believe it is simpler for contracts to store all of their effects in contract state, rather than reporting some through a side channel. Since execution and storage are much cheaper than in a proof-of-work blockchain, this side channel is unnecessary.

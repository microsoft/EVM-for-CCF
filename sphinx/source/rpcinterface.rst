RPC Interface
=============

Where RPC methods have the same name as `Ethereum JSON RPC`_ the app will accept requests in the same format and return compatible responses. Any `default block parameters <https://github.com/ethereum/wiki/wiki/JSON-RPC#the-default-block-parameter>`_ must be ``"latest"`` as the app only has access to the current state.

For example, to retrieve an account's code:

- Request

    .. code-block:: json

        {
            "id":1,
            "jsonrpc": "2.0",
            "method": "eth_getCode",
            "params": [
                "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b",
                "latest"
            ]
        }

- Response

    .. code-block:: json

        {
            "id":1,
            "jsonrpc": "2.0",
            "result": "0x60016008..."
        }

Ethereum addresses, 32-byte numbers and byte strings should be presented hex-encoded, prefixed with '0x'. Any missing params in json will result in an error during deserialization.

.. _rpc_list:

Supported RPCs
--------------

The following RPCs are currently supported:

* ``eth_call``
* ``eth_getBalance``
* ``eth_getCode``
* ``eth_getTransactionCount``
* ``eth_getTransactionReceipt``
* ``eth_sendTransaction``
* ``eth_sendRawTransaction``

Many RPCs are not supported as they break the privacy model. For instance ``eth_getStorageAt`` should not be implemented, as it allows users to read arbitrary state from EVM storage. We want all such access to go through bytecode execution (ie - to call a method on a contract, with potential access controls), so this RPC is not implemented.

Others do not match the execution model more generally. The service is responsible solely for execution, it is not a node owning a specific user identity, so ``eth_accounts`` does not make sense. All RPCs which request block state, events, or gas costs are similarly inapplicable and not implemented.

.. _`Ethereum JSON RPC`: https://github.com/ethereum/wiki/wiki/JSON-RPC

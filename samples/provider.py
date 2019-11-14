# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
import itertools
import web3

# Implementation of web3 Provider which talks framed-JSON-over-TLS to a CCF node
class CCFProvider(web3.providers.BaseProvider):
    def __init__(self, ccf_client, logging=False):
        self.middlewares = []
        self.ccf_client = ccf_client
        if not logging:
            self.disable_logging()
        self.supported_methods = ccf_client.do("listMethods", {}).result["methods"]

    def disable_logging(self):
        self.ccf_client.rpc_loggers = ()

    def make_request(self, method, params):
        if method not in self.supported_methods:
            raise web3.exceptions.CannotHandleRequest(
                f"CCF does not support '{method}'"
            )
        return self.ccf_client.rpc(method, params).to_dict()

    def isConnected(self):
        return self.ccf_client.conn.fileno() != -1

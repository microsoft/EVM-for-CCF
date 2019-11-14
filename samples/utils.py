# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
import binascii
import json
import random
import os
import time

import web3

from loguru import logger as LOG


class Caller:
    def __init__(self, account, w3):
        self.account = account
        self.w3 = w3
        self.default_transaction = {
            "from": self.account.address,
            "gas": 0,
            "gasPrice": 0,
        }

    def _build_transaction(self, fn, **kwargs):
        nonce = self.w3.eth.getTransactionCount(self.account.address)
        txn = fn.buildTransaction({**self.default_transaction, "nonce": nonce})
        txn.update(**kwargs)
        return txn

    def send(self, fn, **kwargs):
        txn = self._build_transaction(fn, **kwargs)
        tx_hash = self.w3.eth.sendTransaction(txn)
        return self.w3.eth.waitForTransactionReceipt(tx_hash)

    def send_signed(self, fn, **kwargs):
        txn = self._build_transaction(fn, **kwargs)
        signed = self.account.signTransaction(txn)
        tx_hash = self.w3.eth.sendRawTransaction(signed.rawTransaction)
        return self.w3.eth.waitForTransactionReceipt(tx_hash)

    def call(self, fn):
        return fn.call(self.default_transaction)


def read_contract_from_file(file_path, contract_element):
    assert os.path.isfile(file_path), f"{file_path} is not a file"
    LOG.info(f"Loading contract from {file_path}")
    with open(file_path) as f:
        file_json = json.load(f)
        j = file_json["contracts"][contract_element]
        contract_abi = json.loads(j["abi"])
        contract_bin = j["bin"]

    return contract_abi, contract_bin


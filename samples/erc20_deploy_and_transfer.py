# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
import json
import os
import web3

from utils import *
import e2e_args
import infra.ccf
import provider

from loguru import logger as LOG


class ERC20Contract:
    def __init__(self, contract):
        self.contract = contract

    def get_total_supply(self, caller):
        return caller.call(self.contract.functions.totalSupply())

    def get_token_balance(self, caller, address=None):
        if address is None:
            address = caller.account.address
        return caller.call(self.contract.functions.balanceOf(address))

    def print_balances(self, callers):
        balances = [
            (caller.account.address, self.get_token_balance(caller))
            for caller in callers
        ]
        for a, b in balances:
            LOG.info(f"{a}: {b}")

    def transfer_tokens(self, sender, to, amount):
        return sender.send_signed(
            self.contract.functions.transfer(to.account.address, amount)
        )

    def transfer_and_check(self, sender, to, amount):
        transfer_receipt = self.transfer_tokens(sender, to, amount)

        events = self.contract.events.Transfer().processReceipt(transfer_receipt)
        for e in events:
            args = e.args
            topic_src = args._from
            topic_dst = args._to
            topic_amt = args._value
            if (
                sender.account.address == topic_src
                and to.account.address == topic_dst
                and amount == topic_amt
            ):
                return True

        return False


def read_erc20_contract_from_file():
    env_name = "CONTRACTS_DIR"
    contracts_dir = os.getenv(env_name)
    if contracts_dir is None:
        raise RuntimeError(f"Cannot find contracts, please set env var '{env_name}'")
    file_path = os.path.join(contracts_dir, "ERC20_combined.json")
    return read_contract_from_file(file_path, "ERC20.sol:ERC20Token")


def test_deploy(network, args):
    erc20_abi, erc20_bin = read_erc20_contract_from_file()
    primary, term = network.find_primary()

    with primary.user_client(format="json") as ccf_client:
        w3 = web3.Web3(provider.CCFProvider(ccf_client))

        owner = Caller(web3.Account.create(), w3)

        LOG.info("Contract deployment")
        erc20_spec = w3.eth.contract(abi=erc20_abi, bytecode=erc20_bin)
        deploy_receipt = owner.send_signed(erc20_spec.constructor(10000))

        network.erc20_contract_address = deploy_receipt.contractAddress
        network.owner_account = owner.account

    return network


def test_transfers(network, args):
    erc20_abi, erc20_bin = read_erc20_contract_from_file()
    primary, term = network.find_primary()

    with primary.user_client(format="json") as ccf_client:
        w3 = web3.Web3(provider.CCFProvider(ccf_client))

        erc20_contract = ERC20Contract(
            w3.eth.contract(abi=erc20_abi, address=network.erc20_contract_address)
        )

        owner = Caller(network.owner_account, w3)
        alice = Caller(web3.Account.create(), w3)
        bob = Caller(web3.Account.create(), w3)

        LOG.info("Get balance of owner")
        owner_balance = erc20_contract.get_token_balance(owner)
        LOG.info(f"Owner balance: {owner_balance}")

        first_amount = owner_balance // 5
        LOG.info(
            "Transferring {} tokens from {} to {}".format(
                first_amount, owner.account.address, alice.account.address
            ),
            True,
        )
        assert erc20_contract.transfer_and_check(owner, alice, first_amount)

        second_amount = owner_balance - first_amount
        LOG.info(
            "Transferring {} tokens from {} to {}".format(
                second_amount, owner.account.address, bob.account.address
            )
        )
        assert erc20_contract.transfer_and_check(owner, bob, second_amount)

        third_amount = second_amount // 3
        LOG.info(
            "Transferring {} tokens from {} to {}".format(
                third_amount, bob.account.address, alice.account.address
            )
        )
        assert erc20_contract.transfer_and_check(bob, alice, third_amount,)

        LOG.info("Balances:")
        erc20_contract.print_balances([owner, alice, bob])

        # Send many transfers, pausing between batches so that notifications should be received
        for batch in range(3):
            for i in range(20):
                sender, receiver = random.sample([alice, bob], 2)
                amount = random.randint(1, 10)
                erc20_contract.transfer_and_check(
                    sender, receiver, amount,
                )
            time.sleep(2)

        LOG.info("Final balances:")
        erc20_contract.print_balances([owner, alice, bob])

    return network


def run(args):
    hosts = ["localhost", "localhost"]

    with infra.ccf.network(
        hosts, args.build_dir, args.debug_nodes, args.perf_nodes, pdb=args.pdb
    ) as network:
        network.start_and_join(args)

        network = test_deploy(network, args)
        network = test_transfers(network, args)


if __name__ == "__main__":
    args = e2e_args.cli_args()
    args.package = "libevm4ccf"

    run(args)

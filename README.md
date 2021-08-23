## ⚠️
#### _This repository is based on a very old version of CCF, and is no longer being updated. For up-to-date sample apps for CCF, see the main CCF repo: https://github.com/Microsoft/CCF_
## ⚠️

# EVM for CCF

This repository contains a sample application for the Confidential Consortium Framework ([CCF](https://github.com/Microsoft/CCF)) running an Ethereum Virtual Machine ([EVM](https://github.com/Microsoft/eEVM/)). This demonstrates how to build CCF from an external project, while also showcasing CCF's deterministic commits, dynamic confidentiality, and high performance.

The app exposes API endpoints based on the [Ethereum JSON RPC](https://github.com/ethereum/wiki/wiki/JSON-RPC) specification (eg - `eth_sendRawTransaction`, `eth_getTransactionReceipt`), so some standard Ethereum tooling can be reused by merely modifying the transport layer to communicate with CCF.

## Contents

| File/folder       | Description                                |
|-------------------|--------------------------------------------|
| `src`             | Source code for the EVM4CCF app            |
| `tests`           | Unit tests for the app's key functionality |
| `samples`         | End-to-end tests, driving an EVM4CCF instance with standard web3.py tools|

## Prerequisites

This sample requires an SGX-enabled VM with CCF's dependencies. Installation of these requirements is described in [CCF's documentation](https://microsoft.github.io/CCF/quickstart/requirements.html#environment-setup).

## Setup

```
git clone --recurse-submodules https://github.com/microsoft/EVM-for-CCF.git
cd EVM-for-CCF
mkdir build
cd build
cmake .. -GNinja
ninja
```

## Running the sample

To run the full test suite:

```
cd build
./tests.sh -VV
```

To launch a local instance for manual testing:

```
cd build
./tests.sh -N
source env/bin/activate
export PYTHONPATH=../CCF/tests
python ../CCF/tests/start_network.py -g ../CCF/src/runtime_config/gov.lua -p libevm4ccf

  ...
  Started CCF network with the following nodes:
    Node [ 0] = 127.163.125.22:40718
    Node [ 1] = 127.40.220.213:32917
    Node [ 2] = 127.42.144.73:40275
```

User transactions can then be submitted as described in the [CCF documentation](https://microsoft.github.io/CCF/users/issue_commands.html), or via [web3.py](https://web3py.readthedocs.io/) with the `CCFProvider` class defined in `samples/provider.py`.

## Key concepts

CCF is a framework for building fault-tolerant, high-performance, fully-confidential distributed services, hosting a user-defined application. In this case the user-defined application is an interpreter for Ethereum bytecode, executing smart contracts entirely inside a [TEE](https://en.wikipedia.org/wiki/Trusted_execution_environment).

This service looks in many ways like a traditional Ethereum node, but has some fundamental differences:
- Consensus is deterministic rather than probabilistic. Since we trust the executing node, we do not need to re-execute on every node or wait for multiple block commits. There is a single transaction history, with no forks.
- There are no local nodes. Users do not run their own node, trusting it with key access and potentially private state. Instead all nodes run inside enclaves, maintaining privacy and guaranteeing execution integrity, regardless of where those enclaves are actually hosted.
- State is confidential, and that confidentiality is entirely controlled by smart contract logic. The app does not produce a public log of all transactions, and it does not reveal the resulting state to all users. The only access to state is by calling methods on smart contracts, where arbitrarily complex and dynamic restrictions can be applied.

## Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.opensource.microsoft.com.

When you submit a pull request, a CLA bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., status check, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

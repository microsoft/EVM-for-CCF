// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "ds/logger.h"
#include "enclave/appinterface.h"
#include "shared.h"

#include <doctest/doctest.h>
#include <eEVM/opcode.h>



using namespace ccf;
using namespace evm4ccf;

TEST_CASE("Call0" * doctest::test_suite("call"))
{
  NetworkTables nwt;
  StubNotifier stubn;
  Store& tables = *nwt.tables;
  auto cert = setup_tables(tables);
  Ethereum frontend = ccfapp::get_rpc_handler(nwt, stubn);

  jsonrpc::SeqNo sn = 0;

  eevm::Address created;

  const auto mem_dest = 0;
  const auto ret_size = 32;
  const auto code = eevm::to_hex_string(std::vector<uint8_t>{
    // Push 5
    eevm::Opcode::PUSH1,
    5,

    // Push 4
    eevm::Opcode::PUSH1,
    4,

    // Add them
    eevm::Opcode::ADD,

    // Store the result info in memory (can only return from memory)
    eevm::Opcode::PUSH1,
    mem_dest,
    eevm::Opcode::MSTORE,

    // Return ret_size bytes, starting at mem_dest
    eevm::Opcode::PUSH1,
    ret_size,
    eevm::Opcode::PUSH1,
    mem_dest,
    eevm::Opcode::RETURN});

  nlohmann::json storage;

  // Create an account
  {
    created = deploy_contract(code, frontend, cert);
  }

  // Code matches argument
  {
    auto in = ethrpc::GetCode::make(sn++);
    in.params.address = created;
    ethrpc::GetCode::Out out = do_rpc(frontend, cert, in);
    REQUIRE(out.result == code);
  }

  // Call
  {
    auto in = ethrpc::Call::make(sn++);
    in.params.call_data.to = created;
    in.params.call_data.data = "0x";
    ethrpc::Call::Out out = do_rpc(frontend, cert, in);
    uint256_t res = get_result_value(out);
    REQUIRE(res == 0x9);
  }
}

TEST_CASE("Call1" * doctest::test_suite("call"))
{
  NetworkTables nwt;
  StubNotifier stubn;
  Store& tables = *nwt.tables;
  auto cert = setup_tables(tables);
  Ethereum frontend = ccfapp::get_rpc_handler(nwt, stubn);

  jsonrpc::SeqNo sn = 0;

  eevm::Address created;

  // See Call1.sol for source
  // f(uint a, uint b) -> a * (b + 42)
  const auto compiled = read_bytecode("Call1");
  const auto code = compiled.runtime;

  nlohmann::json storage;

  // Create instance of contract
  {
    created = deploy_contract(code, frontend, cert);
  }

  // Code matches argument
  {
    auto in = ethrpc::GetCode::make(sn++);
    in.params.address = created;
    ethrpc::GetCode::Out out = do_rpc(frontend, cert, in);
    REQUIRE(out.result == code);
  }

  // Call f(2, 1)
  {
    auto in = ethrpc::Call::make(sn++);
    in.params.call_data.to = created;
    in.params.call_data.data =
      abi_append(compiled.hashes["f(uint256,uint256)"], 2, 1);
    ethrpc::Call::Out out = do_rpc(frontend, cert, in);
    uint256_t res = get_result_value(out);
    REQUIRE(res == 86);
  }
}

TEST_CASE("Call2" * doctest::test_suite("call"))
{
  NetworkTables nwt;
  StubNotifier stubn;
  Store& tables = *nwt.tables;
  auto cert = setup_tables(tables);
  Ethereum frontend = ccfapp::get_rpc_handler(nwt, stubn);
  jsonrpc::SeqNo sn = 0;

  eevm::Address created;
  Balance balance = 0xffff0000;
  // See Call2.sol for source
  // Contains 3 functions, get(), add(uint), mul(uint)
  const auto compiled = read_bytecode("Call2");
  auto code = compiled.runtime;

  nlohmann::json storage;

  // Create instance of contract
  {
    created = deploy_contract(code, frontend, cert);
  }

  // Code matches argument
  {
    auto in = ethrpc::GetCode::make(sn++);
    in.params.address = created;
    ethrpc::GetCode::Out out = do_rpc(frontend, cert, in);
    REQUIRE(out.result == code);
  }

  {
    INFO("call get()");
    auto in = ethrpc::Call::make(sn++);
    in.params.call_data.to = created;
    in.params.call_data.data = compiled.hashes["get()"];
    ethrpc::Call::Out out = do_rpc(frontend, cert, in);
    uint256_t res = get_result_value(out);
    REQUIRE(res == 42);
  }

  {
    INFO("call add(1)");
    auto in = ethrpc::Call::make(sn++);
    in.params.call_data.to = created;
    in.params.call_data.data = abi_append(compiled.hashes["add(uint256)"], 1);
    ethrpc::Call::Out out = do_rpc(frontend, cert, in);
    uint256_t res = get_result_value(out);
    REQUIRE(res == 43);
  }

  {
    INFO("call add(100)");
    auto in = ethrpc::Call::make(sn++);
    in.params.call_data.to = created;
    in.params.call_data.data = abi_append(compiled.hashes["add(uint256)"], 100);
    ethrpc::Call::Out out = do_rpc(frontend, cert, in);
    uint256_t res = get_result_value(out);
    REQUIRE(res == 142);
  }

  {
    INFO("call mul(3)");
    auto in = ethrpc::Call::make(sn++);
    in.params.call_data.to = created;
    in.params.call_data.data = abi_append(compiled.hashes["mul(uint256)"], 3);
    ethrpc::Call::Out out = do_rpc(frontend, cert, in);
    uint256_t res = get_result_value(out);
    REQUIRE(res == 126);
  }

  {
    INFO("call mul(10)");
    auto in = ethrpc::Call::make(sn++);
    in.params.call_data.to = created;
    in.params.call_data.data = abi_append(compiled.hashes["mul(uint256)"], 10);
    ethrpc::Call::Out out = do_rpc(frontend, cert, in);
    uint256_t res = get_result_value(out);
    REQUIRE(res == 420);
  }

  {
    INFO("call mul(100)");
    auto in = ethrpc::Call::make(sn++);
    in.params.call_data.to = created;
    in.params.call_data.data = abi_append(compiled.hashes["mul(uint256)"], 100);
    ethrpc::Call::Out out = do_rpc(frontend, cert, in);
    uint256_t res = get_result_value(out);
    REQUIRE(res == 4200);
  }
}

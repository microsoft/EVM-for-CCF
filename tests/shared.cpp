// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "shared.h"

#include "ds/files.h"
#include "ethereum_transaction.h"

#include <eEVM/opcode.h>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

using namespace ccf;
using namespace evm4ccf;

nlohmann::json unpack(const std::vector<uint8_t>& data)
{
  return unpack(data, s_packType);
}

std::vector<uint8_t> setup_tables(Store& tables)
{
  tables.set_encryptor(std::make_shared<NullTxEncryptor>());

  // Initialise VALUES tables
  ccf::Store::Tx tx;
  auto values = tables.get<ccf::Values>(ccf::Tables::VALUES);
  auto v = tx.get_view(*values);
  for (int id_type = 0; id_type < ccf::ValueIds::END_ID; id_type++)
    v->put(id_type, 0);

  REQUIRE(tx.commit() == kv::CommitSuccess::OK);

  return add_user_cert(tables);
}

uint256_t get_result_value(const std::string& s)
{
  REQUIRE(
    s.size() <=
    66); // Check the result is a single uint256 (hex-encoded string)
  return eevm::to_uint256(s);
}

uint256_t get_result_value(const ethrpc::Call::Out& response)
{
  return get_result_value(response.result);
}

evm4ccf::ByteData make_deployment_code(const evm4ccf::ByteData& runtime_code)
{
  const auto code_bytes = eevm::to_bytes(runtime_code);

  std::vector<uint8_t> deploy_bytecode;

  if (deploy_bytecode.size() > 0xffff)
  {
    throw std::logic_error("This function only handles small code");
  }

  // Reserve space for the code pushed below
  deploy_bytecode.reserve(5 * code_bytes.size() + 5);

  // Store runtime code in memory
  for (uint8_t i = 0u; i < code_bytes.size(); ++i)
  {
    // Push value
    deploy_bytecode.emplace_back(eevm::Opcode::PUSH1);
    deploy_bytecode.emplace_back(code_bytes[i]);

    // Push offset
    deploy_bytecode.emplace_back(eevm::Opcode::PUSH1);
    deploy_bytecode.emplace_back(i);

    // Store byte, popping offset then value
    deploy_bytecode.emplace_back(eevm::Opcode::MSTORE8);
  }

  // Return runtime code from memory
  {
    // Push size
    deploy_bytecode.emplace_back(eevm::Opcode::PUSH1);
    deploy_bytecode.emplace_back((uint8_t)code_bytes.size());

    // Push offset
    deploy_bytecode.emplace_back(eevm::Opcode::PUSH1);
    deploy_bytecode.emplace_back(0u);

    // Return, popping offset then size
    deploy_bytecode.emplace_back(eevm::Opcode::RETURN);
  }

  return eevm::to_hex_string(deploy_bytecode);
}

CompiledBytecode read_bytecode(const std::string& contract_name)
{
  constexpr auto env_var = "CONTRACTS_DIR";
  const auto contracts_dir = getenv(env_var);
  if (!contracts_dir)
  {
    throw std::logic_error(
      "Test is trying to read contract '" + contract_name +
      "', but environment var " + env_var + " is not set");
  }

  const std::string contract_path =
    std::string(contracts_dir) + "/" + contract_name + "_combined.json";

  const auto j = files::slurp_json(contract_path);

  const std::string element_id = contract_name + ".sol:" + contract_name;
  const auto contract_element = j["contracts"][element_id];
  const evm4ccf::ByteData deploy =
    "0x" + contract_element["bin"].get<std::string>();
  const evm4ccf::ByteData runtime =
    "0x" + contract_element["bin-runtime"].get<std::string>();
  const auto hashes = contract_element["hashes"];
  return {deploy, runtime, hashes};
}

nlohmann::json do_rpc(
  Ethereum& handler,
  const std::vector<uint8_t>& cert,
  nlohmann::json pc,
  bool success)
{
  auto rpc = pc.dump();
  INFO("Sending RPC: " << rpc);

  const enclave::SessionContext session(0, cert);
  auto packed = pack(pc);
  const auto rpc_ctx = enclave::make_rpc_context(session, packed);
  auto r = handler->process(rpc_ctx);
  auto j = unpack(r.value());

  auto response = j.dump();
  INFO("Response: " << response);

  if (j.find(std::string(jsonrpc::ERR)) != j.end())
  {
    REQUIRE_FALSE(success);
  }
  else
  {
    REQUIRE(success);
  }

  return j;
}

TestAccount::TestAccount(Ethereum ef, ccf::Store& tables) :
  frontend(ef),
  sn(0),
  privk(std::make_unique<tls::KeyPair_k1Bitcoin>(MBEDTLS_ECP_DP_SECP256K1)),
  cert(add_user_cert(tables))
{
  address = evm4ccf::get_address_from_public_key_asn1(privk->public_key_asn1());
}

ByteData TestAccount::get_code(const eevm::Address& contract)
{
  auto in = ethrpc::GetCode::make(sn++);
  in.params.address = contract;
  ethrpc::GetCode::Out out = do_rpc(frontend, cert, in);

  return out.result;
}

eevm::Address TestAccount::deploy_contract(
  const ByteData& code, evm4ccf::TxHash* o_deploy_hash)
{
  auto send_in = ethrpc::SendTransaction::make(sn++);
  send_in.params.call_data.from = address;
  send_in.params.call_data.data = code;
  ethrpc::SendTransaction::Out send_out = do_rpc(frontend, cert, send_in);
  const auto tx_hash = send_out.result;

  auto get_receipt_in = ethrpc::GetTransactionReceipt::make(sn++);
  get_receipt_in.params.tx_hash = tx_hash;
  ethrpc::GetTransactionReceipt::Out get_receipt_out =
    do_rpc(frontend, cert, get_receipt_in);
  REQUIRE(get_receipt_out.result.has_value());

  if (o_deploy_hash != nullptr)
    *o_deploy_hash = tx_hash;

  return get_receipt_out.result->contract_address.value();
}

DeployedContract TestAccount::deploy_private_contract(
  const ByteData& code,
  const ContractParticipants& participants,
  evm4ccf::TxHash* o_deploy_hash)
{
  auto send_in = ethrpc::SendTransaction::make(sn++);
  send_in.params.call_data.from = address;
  send_in.params.call_data.data = code;
  send_in.params.call_data.private_for = participants;

  ethrpc::SendTransaction::Out send_out = do_rpc(frontend, cert, send_in);
  const auto tx_hash = send_out.result;

  auto get_receipt_in = ethrpc::GetTransactionReceipt::make(sn++);
  get_receipt_in.params.tx_hash = tx_hash;
  ethrpc::GetTransactionReceipt::Out get_receipt_out =
    do_rpc(frontend, cert, get_receipt_in);
  REQUIRE(get_receipt_out.result.has_value());

  if (o_deploy_hash != nullptr)
    *o_deploy_hash = tx_hash;

  return DeployedContract(
    get_receipt_out.result->contract_address.value(), participants);
}

nlohmann::json TestAccount::contract_transact_raw(
  const DeployedContract& contract,
  const ByteData& code,
  bool expect_success /* = true*/)
{
  auto in = ethrpc::SendTransaction::make(sn++);
  in.params.call_data.from = address;
  in.params.call_data.to = contract.address;
  in.params.call_data.data = code;

  if (contract.participants.has_value())
  {
    in.params.call_data.private_for = contract.participants;
  }

  return do_rpc(frontend, cert, in, expect_success);
}

nlohmann::json TestAccount::contract_call_raw(
  const DeployedContract& contract,
  const ByteData& code,
  bool expect_success /* = true*/)
{
  auto in = ethrpc::Call::make(sn++);
  in.params.call_data.from = address;
  in.params.call_data.to = contract.address;
  in.params.call_data.data = code;

  if (contract.participants.has_value())
  {
    in.params.call_data.private_for = contract.participants;
  }

  return do_rpc(frontend, cert, in, expect_success);
}

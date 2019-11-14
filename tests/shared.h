// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "node/encryptor.h"
#include "node/networkstate.h"
#include "rpc_types.h"

#include <enclave/appinterface.h>
#include <kv/kv.h>
#include <nlohmann/json.hpp>

constexpr jsonrpc::Pack s_packType = jsonrpc::Pack::MsgPack;
using Ethereum = std::shared_ptr<enclave::RpcHandler>;

template <typename T>
auto pack(const T& v)
{
  return jsonrpc::pack(v, s_packType);
}

nlohmann::json unpack(const std::vector<uint8_t>& data);

std::vector<uint8_t> setup_tables(ccf::Store& tables);

inline std::vector<uint8_t> add_user_cert(ccf::Store& tables)
{
  // create a new cert
  auto kp = tls::make_key_pair();
  auto ca = kp->self_sign("CN=name");
  auto verifier = tls::make_verifier(ca);
  const auto data_first = verifier->raw()->raw.p;
  const auto data_last = data_first + verifier->raw()->raw.len;
  const auto cert = std::vector<uint8_t>(data_first, data_last);

  // add cert as a user
  auto certs = tables.get<ccf::Certs>(ccf::Tables::USER_CERTS);
  auto values = tables.get<ccf::Values>(ccf::Tables::VALUES);

  ccf::Store::Tx tx;

  const auto user_id =
    ccf::get_next_id(tx.get_view(*values), ccf::ValueIds::NEXT_USER_ID);

  auto user_certs_view = tx.get_view(*certs);
  user_certs_view->put(cert, user_id);
  if (tx.commit() != kv::CommitSuccess::OK)
  {
    throw std::runtime_error("user creation tx failed");
  }

  return cert;
}

nlohmann::json do_rpc(
  Ethereum& handler,
  const std::vector<uint8_t>& cert,
  nlohmann::json pc,
  bool success = true);

uint256_t get_result_value(const std::string& s);
uint256_t get_result_value(const evm4ccf::ethrpc::Call::Out& response);

evm4ccf::ByteData make_deployment_code(const evm4ccf::ByteData& runtime_code);

void make_service_identity(
  ccf::Store& tables, Ethereum& frontend, const ccf::Cert& cert);

template <typename... Ts>
eevm::Address deploy_contract(const evm4ccf::ByteData& runtime_code, Ts&&... ts)
{
  size_t sn = 0;
  auto send_in = evm4ccf::ethrpc::SendTransaction::make(sn++);
  send_in.params.call_data.from = 0x01234;
  send_in.params.call_data.data = make_deployment_code(runtime_code);
  const evm4ccf::ethrpc::SendTransaction::Out send_out =
    do_rpc(std::forward<Ts>(ts)..., send_in);

  auto get_in = evm4ccf::ethrpc::GetTransactionReceipt::make(sn++);
  get_in.params.tx_hash = send_out.result;
  const evm4ccf::ethrpc::GetTransactionReceipt::Out get_out =
    do_rpc(std::forward<Ts>(ts)..., get_in);
  return get_out.result->contract_address.value();
}

struct CompiledBytecode
{
  evm4ccf::ByteData deploy;
  evm4ccf::ByteData runtime;
  nlohmann::json hashes;
};
CompiledBytecode read_bytecode(const std::string& contract_name);

struct DeployedContract
{
  eevm::Address address;
  std::optional<evm4ccf::ContractParticipants> participants;

  DeployedContract(const eevm::Address& a) :
    address(a),
    participants(std::nullopt)
  {}

  DeployedContract(
    const eevm::Address& a, const evm4ccf::ContractParticipants& ps) :
    address(a),
    participants(ps)
  {}
};

class TestAccount
{
  Ethereum frontend;
  jsonrpc::SeqNo sn;
  std::unique_ptr<tls::KeyPair_k1Bitcoin> privk;

public:
  std::vector<uint8_t> cert;
  eevm::Address address;

  TestAccount(Ethereum ef, ccf::Store& tables);

  evm4ccf::ByteData get_code(const eevm::Address& contract);

  eevm::Address deploy_contract(
    const evm4ccf::ByteData& code, evm4ccf::TxHash* o_deploy_hash = nullptr);

  DeployedContract deploy_private_contract(
    const evm4ccf::ByteData& code,
    const evm4ccf::ContractParticipants& participants,
    evm4ccf::TxHash* o_deploy_hash = nullptr);

  nlohmann::json contract_transact_raw(
    const DeployedContract& contract,
    const evm4ccf::ByteData& code,
    bool expect_success = true);

  evm4ccf::TxHash contract_transact(
    const DeployedContract& contract, const evm4ccf::ByteData& code)
  {
    evm4ccf::ethrpc::SendTransaction::Out out =
      contract_transact_raw(contract, code);
    return out.result;
  }

  nlohmann::json contract_call_raw(
    const DeployedContract& contract,
    const evm4ccf::ByteData& code,
    bool expect_success = true);

  evm4ccf::ByteData contract_call(
    const DeployedContract& contract, const evm4ccf::ByteData& code)
  {
    evm4ccf::ethrpc::Call::Out out = contract_call_raw(contract, code);
    return out.result;
  }
};

class StubNotifier : public ccf::AbstractNotifier
{
  void notify(const std::vector<uint8_t>& data) override {}
};

// Construct ABI-encoded function calls
template <typename T, typename... Ts>
std::string abi_append(const std::string& base, const T& t, Ts&&... ts)
{
  const auto hexed = eevm::to_hex_string(t);
  const auto abi = fmt::format("{}{:0>64}", base, hexed.substr(2));

  if constexpr (sizeof...(Ts) == 0)
  {
    return abi;
  }
  else
  {
    return abi_append(abi, std::forward<Ts>(ts)...);
  }
}

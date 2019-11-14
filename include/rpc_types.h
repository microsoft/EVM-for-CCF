// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <eEVM/address.h>
#include <eEVM/bigint.h>
#include <eEVM/transaction.h>
#include <eEVM/util.h>
#include <enclave/appinterface.h>
#include <kv/kv.h>
#include <node/rpc/jsonrpc.h>

// STL
#include <array>
#include <vector>

namespace evm4ccf
{
  using Balance = uint256_t;

  using BlockID = std::string;
  constexpr auto DefaultBlockID = "latest";

  // Pass around hex-encoded strings that we can manipulate as long as possible,
  // only convert to actual byte arrays when needed
  using ByteData = std::string;

  using EthHash = uint256_t;
  using TxHash = EthHash;
  using BlockHash = EthHash;

  using ContractParticipants = std::set<eevm::Address>;

  // TODO(eddy|#refactoring): Reconcile this with eevm::Block
  struct BlockHeader
  {
    uint64_t number = {};
    uint64_t difficulty = {};
    uint64_t gas_limit = {};
    uint64_t gas_used = {};
    uint64_t timestamp = {};
    eevm::Address miner = {};
    BlockHash block_hash = {};
  };

  inline bool operator==(const BlockHeader& l, const BlockHeader& r)
  {
    return l.number == r.number && l.difficulty == r.difficulty &&
      l.gas_limit == r.gas_limit && l.gas_used == r.gas_used &&
      l.timestamp == r.timestamp && l.miner == r.miner &&
      l.block_hash == r.block_hash;
  }

  namespace rpcparams
  {
    struct MessageCall
    {
      eevm::Address from = {};
      std::optional<eevm::Address> to = std::nullopt;
      uint256_t gas = 90000;
      uint256_t gas_price = 0;
      uint256_t value = 0;
      ByteData data = {};
      std::optional<ContractParticipants> private_for = std::nullopt;
    };

    struct AddressWithBlock
    {
      eevm::Address address = {};
      BlockID block_id = DefaultBlockID;
    };

    struct Call
    {
      MessageCall call_data = {};
      BlockID block_id = DefaultBlockID;
    };

    struct GetTransactionCount
    {
      eevm::Address address = {};
      BlockID block_id = DefaultBlockID;
    };

    struct GetTransactionReceipt
    {
      TxHash tx_hash = {};
    };

    struct SendRawTransaction
    {
      ByteData raw_transaction = {};
    };

    struct SendTransaction
    {
      MessageCall call_data = {};
    };
  } // namespace rpcparams

  namespace rpcresults
  {
    struct TxReceipt
    {
      TxHash transaction_hash = {};
      uint256_t transaction_index = {};
      BlockHash block_hash = {};
      uint256_t block_number = {};
      eevm::Address from = {};
      std::optional<eevm::Address> to = std::nullopt;
      uint256_t cumulative_gas_used = {};
      uint256_t gas_used = {};
      std::optional<eevm::Address> contract_address = std::nullopt;
      std::vector<eevm::LogEntry> logs = {};
      // logs_bloom could be bitset for interaction, but is currently ignored
      std::array<uint8_t, 256> logs_bloom = {};
      uint256_t status = {};
    };

    // "A transaction receipt object, or null when no receipt was found"
    using ReceiptResponse = std::optional<TxReceipt>;
  } // namespace rpcresults

  template <class TTag, typename TParams, typename TResult>
  struct RpcBuilder
  {
    using Tag = TTag;
    using Params = TParams;
    using Result = TResult;

    using In = jsonrpc::ProcedureCall<TParams>;
    using Out = jsonrpc::Response<TResult>;

    static constexpr auto name = Tag::name;

    static In make(jsonrpc::SeqNo n = 0)
    {
      In in;
      in.id = n;
      in.method = TTag::name;
      return in;
    }
  };

  // Ethereum JSON-RPC
  namespace ethrpc
  {
    struct BlockNumberTag
    {
      static constexpr auto name = "eth_blockNumber";
    };
    using BlockNumber = RpcBuilder<BlockNumberTag, void, ByteData>;

    struct CallTag
    {
      static constexpr auto name = "eth_call";
    };
    using Call = RpcBuilder<CallTag, rpcparams::Call, ByteData>;

    struct GetAccountsTag
    {
      static constexpr auto name = "eth_accounts";
    };
    using GetAccounts =
      RpcBuilder<GetAccountsTag, void, std::vector<eevm::Address>>;

    struct GetBalanceTag
    {
      static constexpr auto name = "eth_getBalance";
    };
    using GetBalance =
      RpcBuilder<GetBalanceTag, rpcparams::AddressWithBlock, Balance>;

    struct GetCodeTag
    {
      static constexpr auto name = "eth_getCode";
    };
    using GetCode =
      RpcBuilder<GetCodeTag, rpcparams::AddressWithBlock, ByteData>;

    struct GetTransactionCountTag
    {
      static constexpr auto name = "eth_getTransactionCount";
    };
    using GetTransactionCount = RpcBuilder<
      GetTransactionCountTag,
      rpcparams::GetTransactionCount,
      size_t>;

    struct GetTransactionReceiptTag
    {
      static constexpr auto name = "eth_getTransactionReceipt";
    };
    using GetTransactionReceipt = RpcBuilder<
      GetTransactionReceiptTag,
      rpcparams::GetTransactionReceipt,
      rpcresults::ReceiptResponse>;

    struct SendRawTransactionTag
    {
      static constexpr auto name = "eth_sendRawTransaction";
    };
    using SendRawTransaction =
      RpcBuilder<SendRawTransactionTag, rpcparams::SendRawTransaction, TxHash>;

    struct SendTransactionTag
    {
      static constexpr auto name = "eth_sendTransaction";
    };
    using SendTransaction =
      RpcBuilder<SendTransactionTag, rpcparams::SendTransaction, TxHash>;
  } // namespace ethrpc
} // namespace evm4ccf

#include "rpc_types_serialization.inl"

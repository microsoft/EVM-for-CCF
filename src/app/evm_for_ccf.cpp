// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// EVM-for-CCF
#include "account_proxy.h"
#include "ethereum_state.h"
#include "ethereum_transaction.h"
#include "tables.h"

// CCF
#include "ds/hash.h"
#include "node/rpc/nodeinterface.h"
#include "node/rpc/userfrontend.h"
#include "rpc_types.h"

// eEVM
#include <eEVM/address.h>
#include <eEVM/bigint.h>
#include <eEVM/processor.h>
#include <eEVM/rlp.h>
#include <eEVM/util.h>

// STL/3rd-party
#include <msgpack-c/msgpack.hpp>

namespace evm4ccf
{
  using namespace std;
  using namespace eevm;
  using namespace ccf;
  using namespace ccfapp;

  //
  // RPC handler class
  //
  class EVMForCCFFrontend : public UserRpcFrontend
  {
    tables::Accounts accounts;
    tables::Storage& storage;
    tables::Results& tx_results;

    EthereumState make_state(Store::Tx& tx)
    {
      return EthereumState(accounts.get_views(tx), tx.get_view(storage));
    }

    void install_standard_rpcs()
    {
      auto call = [this](RequestArgs& args) {
        CallerId caller_id = args.caller_id;
        Store::Tx& tx = args.tx;
        ethrpc::Call::Params cp = args.params;

        if (!cp.call_data.to.has_value())
        {
          return jsonrpc::error(
            jsonrpc::StandardErrorCodes::INVALID_PARAMS, "Missing 'to' field");
        }

        auto es = make_state(tx);

        const auto e = run_in_evm(cp.call_data, es).first;

        if (e.er == ExitReason::returned || e.er == ExitReason::halted)
        {
          // Call should have no effect so we don't commit it.
          // Just return the result.
          return jsonrpc::success(to_hex_string(e.output));
        }
        else
        {
          return jsonrpc::error(
            jsonrpc::StandardErrorCodes::INTERNAL_ERROR, e.exmsg);
        }
      };

      auto get_balance = [this](Store::Tx& tx, const nlohmann::json& params) {
        rpcparams::AddressWithBlock ab = params;
        if (ab.block_id != "latest")
        {
          return jsonrpc::error(
            jsonrpc::StandardErrorCodes::INVALID_PARAMS,
            "Can only request latest block");
        }

        auto es = make_state(tx);

        const auto account_state = es.get(ab.address);
        return jsonrpc::success(to_hex_string(account_state.acc.get_balance()));
      };

      auto get_code = [this](Store::Tx& tx, const nlohmann::json& params) {
        rpcparams::AddressWithBlock ab = params;
        if (ab.block_id != "latest")
        {
          return jsonrpc::error(
            jsonrpc::StandardErrorCodes::INVALID_PARAMS,
            "Can only request latest block");
        }

        auto es = make_state(tx);

        const auto account_state = es.get(ab.address);
        return jsonrpc::success(to_hex_string(account_state.acc.get_code()));
      };

      auto get_transaction_count =
        [this](Store::Tx& tx, const nlohmann::json& params) {
          rpcparams::GetTransactionCount gtcp = params;
          if (gtcp.block_id != "latest")
          {
            return jsonrpc::error(
              jsonrpc::StandardErrorCodes::INVALID_PARAMS,
              "Can only request latest block");
          }

          auto es = make_state(tx);
          auto account_state = es.get(gtcp.address);

          return jsonrpc::success(to_hex_string(account_state.acc.get_nonce()));
        };

      auto send_raw_transaction = [this](RequestArgs& args) {
        rpcparams::SendRawTransaction srtp = args.params;

        eevm::rlp::ByteString in = eevm::to_bytes(srtp.raw_transaction);

        EthereumTransactionWithSignature eth_tx(in);

        rpcparams::MessageCall tc;
        eth_tx.to_transaction_call(tc);

        return execute_transaction(args.caller_id, tc, args.tx);
      };

      auto send_transaction = [this](RequestArgs& args) {
        rpcparams::SendTransaction stp = args.params;

        return execute_transaction(args.caller_id, stp.call_data, args.tx);
      };

      auto get_transaction_receipt =
        [this](Store::Tx& tx, const nlohmann::json& params) {
          rpcparams::GetTransactionReceipt gtrp = params;

          const TxHash& tx_hash = gtrp.tx_hash;

          auto results_view = tx.get_view(tx_results);
          const auto r = results_view->get(tx_hash);

          // "or null when no receipt was found"
          rpcresults::ReceiptResponse response = nullopt;
          if (r.has_value())
          {
            const auto& tx_result = r.value();

            response.emplace();
            response->transaction_hash = tx_hash;
            if (tx_result.contract_address.has_value())
            {
              response->contract_address = tx_result.contract_address;
            }
            else
            {
              response->to = 0x0;
            }
            response->logs = tx_result.logs;
            response->status = 1;
          }

          return jsonrpc::success(response);
        };

      install(ethrpc::Call::name, call, Read);
      install(ethrpc::GetBalance::name, get_balance, Read);
      install(ethrpc::GetCode::name, get_code, Read);
      install(ethrpc::GetTransactionCount::name, get_transaction_count, Read);
      install(
        ethrpc::GetTransactionReceipt::name, get_transaction_receipt, Read);
      install(ethrpc::SendRawTransaction::name, send_raw_transaction, Write);
      install(ethrpc::SendTransaction::name, send_transaction, Write);
    }

  public:
    // SNIPPET_START: initialization
    EVMForCCFFrontend(NetworkTables& nwt, AbstractNotifier& notifier) :
      UserRpcFrontend(*nwt.tables),
      accounts{tables.create<tables::Accounts::Balances>("eth.account.balance"),
               tables.create<tables::Accounts::Codes>("eth.account.code"),
               tables.create<tables::Accounts::Nonces>("eth.account.nonce")},
      storage(tables.create<tables::Storage>("eth.storage")),
      tx_results(tables.create<tables::Results>("eth.txresults"))
    // SNIPPET_END: initialization
    {
      install_standard_rpcs();
    }

  private:
    static std::pair<ExecResult, AccountState> run_in_evm(
      const rpcparams::MessageCall& call_data,
      EthereumState& es,
      LogHandler& log_handler)
    {
      Address from = call_data.from;
      Address to;

      if (call_data.to.has_value())
      {
        to = call_data.to.value();
      }
      else
      {
        // If there's no to field, create a new account to deploy this to
        const auto from_state = es.get(from);
        to = eevm::generate_address(
          from_state.acc.get_address(), from_state.acc.get_nonce());
        es.create(to, call_data.gas, to_bytes(call_data.data));
      }

      Transaction eth_tx(from, log_handler);

      auto account_state = es.get(to);

#ifdef RECORD_TRACE
      eevm::Trace tr;
#endif

      Processor proc(es);
      const auto result = proc.run(
        eth_tx,
        from,
        account_state,
        to_bytes(call_data.data),
        call_data.value
#ifdef RECORD_TRACE
        ,
        &tr
#endif
      );

#ifdef RECORD_TRACE
      if (result.er == ExitReason::threw)
      {
        LOG_INFO_FMT("--- Trace of failing evm execution ---\n{}", tr);
      }
#endif

      return std::make_pair(result, account_state);
    }

    static pair<ExecResult, AccountState> run_in_evm(
      const rpcparams::MessageCall& call_data, EthereumState& es)
    {
      NullLogHandler ignore;
      return run_in_evm(call_data, es, ignore);
    }

    // TODO: This and similar should take EthereumTransaction, not
    // MessageCall. EthereumTransaction should be fully parsed, then
    // MessageCall can be removed
    pair<bool, nlohmann::json> execute_transaction(
      CallerId caller_id,
      const rpcparams::MessageCall& call_data,
      Store::Tx& tx)
    {
      auto es = make_state(tx);

      VectorLogHandler vlh;
      const auto [exec_result, tx_hash, to_address] =
        execute_transaction(call_data, es, vlh);

      if (exec_result.er == ExitReason::threw)
      {
        return jsonrpc::error(
          jsonrpc::StandardErrorCodes::INTERNAL_ERROR, exec_result.exmsg);
      }

      auto results_view = tx.get_view(tx_results);
      TxResult tx_result;
      if (!call_data.to.has_value())
      {
        tx_result.contract_address = to_address;
      }

      tx_result.logs = vlh.logs;

      results_view->put(tx_hash, tx_result);

      return jsonrpc::success(eevm::to_hex_string_fixed(tx_hash));
    }

    static std::tuple<ExecResult, TxHash, Address> execute_transaction(
      const rpcparams::MessageCall& call_data,
      EthereumState& es,
      LogHandler& log_handler)
    {
      auto [exec_result, account_state] =
        run_in_evm(call_data, es, log_handler);

      if (exec_result.er == ExitReason::threw)
      {
        return std::make_tuple(exec_result, 0, 0);
      }

      if (!call_data.to.has_value())
      {
        // New contract created, result is the code that should be deployed
        account_state.acc.set_code(std::move(exec_result.output));
      }

      auto from_state = es.get(call_data.from);
      auto tx_nonce = from_state.acc.get_nonce();
      from_state.acc.increment_nonce();

      EthereumTransaction eth_tx(tx_nonce, call_data);
      const auto rlp_encoded = eth_tx.encode();

      uint8_t h[32];
      const auto raw =
        reinterpret_cast<unsigned char const*>(rlp_encoded.data());
      eevm::keccak_256(raw, rlp_encoded.size(), h);

      const auto tx_hash = eevm::from_big_endian(h);

      return std::make_tuple(
        exec_result, tx_hash, account_state.acc.get_address());
    }
  }; // namespace evm4ccf
} // namespace evm4ccf

namespace ccfapp
{
  // SNIPPET_START: rpc_handler
  std::shared_ptr<enclave::RpcHandler> get_rpc_handler(
    ccf::NetworkTables& nwt, ccf::AbstractNotifier& notifier)
  {
    return std::make_shared<evm4ccf::EVMForCCFFrontend>(nwt, notifier);
  }
  // SNIPPET_END: rpc_handler
} // namespace ccfapp

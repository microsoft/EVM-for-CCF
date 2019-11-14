// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

// EVM-for-CCF
#include "rpc_types.h"

// CCF
#include "ds/hash.h"
#include "enclave/appinterface.h"

// eEVM
#include <eEVM/account.h>
#include <eEVM/address.h>

// STL/3rd-party
#include <vector>

namespace evm4ccf
{
  struct TxResult
  {
    std::optional<eevm::Address> contract_address;
    std::vector<eevm::LogEntry> logs;
  };
} // namespace evm4ccf

#include "msgpacktypes.h"
#include "nljsontypes.h"

// Implement std::hash for uint256, so it can be used as key in kv
namespace std
{
  template <unsigned N>
  struct hash<intx::uint<N>>
  {
    size_t operator()(const intx::uint<N>& n) const
    {
      const auto words = intx::to_words<size_t>(n);
      return hash_container(words);
    }
  };
} // namespace std

namespace evm4ccf
{
  inline bool operator==(const TxResult& l, const TxResult& r)
  {
    return l.contract_address == r.contract_address && l.logs == r.logs;
  }

  namespace tables
  {
    struct Accounts
    {
      using Balances = ccf::Store::Map<eevm::Address, uint256_t>;
      Balances& balances;

      using Codes = ccf::Store::Map<eevm::Address, eevm::Code>;
      Codes& codes;

      using Nonces = ccf::Store::Map<eevm::Address, eevm::Account::Nonce>;
      Nonces& nonces;

      struct Views
      {
        Balances::TxView* balances;
        Codes::TxView* codes;
        Nonces::TxView* nonces;
      };

      Views get_views(ccf::Store::Tx& tx)
      {
        return {tx.get_view(balances), tx.get_view(codes), tx.get_view(nonces)};
      }
    };

    using StorageKey = std::pair<eevm::Address, uint256_t>;
    using Storage = ccf::Store::Map<StorageKey, uint256_t>;

    using Results = ccf::Store::Map<TxHash, TxResult>;
  } // namespace tables
} // namespace evm4ccf

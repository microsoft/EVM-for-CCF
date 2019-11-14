// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

// EVM-for-CCF
#include "tables.h"

// CCF
#include "crypto/hash.h"

// eEVM
#include <eEVM/bigint.h>
#include <eEVM/globalstate.h>
#include <eEVM/rlp.h>

// STL/3rd-party
#include <unordered_map>

namespace evm4ccf
{
  // Implementation of eevm::GlobalState backed by ccf's KV
  class EthereumState : public eevm::GlobalState
  {
    eevm::Block current_block = {};

    tables::Accounts::Views accounts;
    tables::Storage::TxView& tx_storage;

    std::map<eevm::Address, std::unique_ptr<AccountProxy>> cache;

    eevm::AccountState add_to_cache(const eevm::Address& address)
    {
      auto ib = cache.insert(std::make_pair(
        address,
        std::make_unique<AccountProxy>(address, accounts, tx_storage)));

      if (!ib.second)
      {
        throw std::logic_error(fmt::format(
          "Added account proxy to cache at address {}, but an "
          "entry already existed",
          eevm::to_checksum_address(address)));
      }

      auto& proxy = ib.first->second;
      return eevm::AccountState(*proxy, *proxy);
    }

  public:
    template <typename... Ts>
    EthereumState(
      const tables::Accounts::Views& acc_views,
      tables::Storage::TxView* views) :
      accounts(acc_views),
      tx_storage(*views)
    {}

    void remove(const eevm::Address& addr) override
    {
      throw std::logic_error("not implemented");
    }

    eevm::AccountState get(const eevm::Address& address) override
    {
      // If account is already in cache, it can be returned
      auto cache_it = cache.find(address);
      if (cache_it != cache.end())
      {
        auto& proxy = cache_it->second;
        return eevm::AccountState(*proxy, *proxy);
      }

      // If account doesn't already exist, it should be created
      const auto kind_it = accounts.balances->get(address);
      if (!kind_it.has_value())
      {
        return create(address, 0, {});
      }

      // Account exists in kv but not in cache - add a proxy for it
      return add_to_cache(address);
    }

    eevm::AccountState create(
      const eevm::Address& address,
      const uint256_t& balance = 0u,
      const eevm::Code& code = {}) override
    {
      eevm::Account::Nonce initial_nonce = 0;
      if (!code.empty())
      {
        // Nonce of contracts should start at 1
        initial_nonce = 1;
      }

      // Write initial balance
      const auto balance_it = accounts.balances->get(address);
      if (balance_it.has_value())
      {
        throw std::logic_error(fmt::format(
          "Trying to create account at {}, but it already has a balance",
          eevm::to_checksum_address(address)));
      }
      else
      {
        accounts.balances->put(address, balance);
      }

      // Write initial code
      const auto code_it = accounts.codes->get(address);
      if (code_it.has_value())
      {
        throw std::logic_error(fmt::format(
          "Trying to create account at {}, but it already has code",
          eevm::to_checksum_address(address)));
      }
      else
      {
        accounts.codes->put(address, code);
      }

      // Write initial nonce
      const auto nonce_it = accounts.nonces->get(address);
      if (nonce_it.has_value())
      {
        throw std::logic_error(fmt::format(
          "Trying to create account at {}, but it already has a nonce",
          eevm::to_checksum_address(address)));
      }
      else
      {
        accounts.nonces->put(address, initial_nonce);
      }

      return add_to_cache(address);
    }

    const eevm::Block& get_current_block() override
    {
      return current_block;
    }

    uint256_t get_block_hash(uint8_t offset) override
    {
      return 0;
    }
  };
} // namespace evm4ccf

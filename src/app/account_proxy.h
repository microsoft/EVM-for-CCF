// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

// EVM-for-CCF
#include "tables.h"

// eEVM
#include <eEVM/account.h>
#include <eEVM/storage.h>

namespace evm4ccf
{
  // This implements both eevm::Account and eevm::Storage via ccf's KV
  struct AccountProxy : public eevm::Account, public eevm::Storage
  {
    eevm::Address address;
    mutable tables::Accounts::Views accounts_views;
    tables::Storage::TxView& storage;

    AccountProxy(
      const eevm::Address& a,
      const tables::Accounts::Views& av,
      tables::Storage::TxView& st) :
      address(a),
      accounts_views(av),
      storage(st)
    {}

    // Implementation of eevm::Account
    eevm::Address get_address() const override
    {
      return address;
    }

    uint256_t get_balance() const override
    {
      return accounts_views.balances->get(address).value_or(0);
    }

    void set_balance(const uint256_t& b) override
    {
      accounts_views.balances->put(address, b);
    }

    Nonce get_nonce() const override
    {
      return accounts_views.nonces->get(address).value_or(0);
    }

    void increment_nonce() override
    {
      auto nonce = get_nonce();
      ++nonce;
      accounts_views.nonces->put(address, nonce);
    }

    eevm::Code get_code() const override
    {
      return accounts_views.codes->get(address).value_or(eevm::Code{});
    }

    void set_code(eevm::Code&& c) override
    {
      accounts_views.codes->put(address, c);
    }

    // Implementation of eevm::Storage
    std::pair<eevm::Address, uint256_t> translate(const uint256_t& key)
    {
      return std::make_pair(address, key);
    }

    // SNIPPET_START: store_impl
    void store(const uint256_t& key, const uint256_t& value) override
    {
      storage.put(translate(key), value);
    }
    // SNIPPET_END: store_impl

    uint256_t load(const uint256_t& key) override
    {
      return storage.get(translate(key)).value_or(0);
    }

    bool remove(const uint256_t& key) override
    {
      return storage.remove(translate(key));
    }
  };
} // namespace evm4ccf

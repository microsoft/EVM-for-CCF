// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "rpc_types.h"

// CCF
#include "tls/keypair.h"

#include <eEVM/rlp.h>
#include <eEVM/util.h>

namespace evm4ccf
{
  struct ChainIDs
  {
    static constexpr size_t pre_eip_155 = 0;
    static constexpr size_t ethereum_mainnet = 1;
    static constexpr size_t expanse_mainnet = 2;
    static constexpr size_t ropsten = 3;
    static constexpr size_t rinkeby = 4;
    static constexpr size_t goerli = 5;
    static constexpr size_t kovan = 42;
    static constexpr size_t geth_private_default = 1337;
  };

  // TODO: This may become constexpr, determined at compile time. For now it
  // is malleable.
  static size_t current_chain_id = ChainIDs::pre_eip_155;

  static constexpr size_t pre_155_v_start = 27;
  static constexpr size_t post_155_v_start = 35;

  inline bool is_pre_eip_155(size_t v)
  {
    return v == 27 || v == 28;
  }

  inline size_t to_ethereum_recovery_id(size_t rec_id)
  {
    if (rec_id > 3)
    {
      throw std::logic_error(fmt::format(
        "ECDSA recovery values should be between 0 and 3, {} is invalid",
        rec_id));
    }

    if (rec_id > 1)
    {
      throw std::logic_error(fmt::format(
        "Ethereum only accepts finite curve coordinates, {} represents an "
        "infinite value",
        rec_id));
    }

    if (current_chain_id == ChainIDs::pre_eip_155)
    {
      return rec_id + pre_155_v_start;
    }

    return rec_id + current_chain_id * 2 + post_155_v_start;
  }

  inline size_t from_ethereum_recovery_id(size_t v)
  {
    if (is_pre_eip_155(v))
    {
      return v - pre_155_v_start;
    }

    constexpr auto min_valid_v = 37u;
    if (v < min_valid_v)
    {
      throw std::logic_error(fmt::format(
        "Expected v to encode a valid chain ID (must be at least {}), but is "
        "{}",
        min_valid_v,
        v));
    }

    const size_t rec_id = (v - post_155_v_start) % 2;

    const size_t chain_id = ((v - rec_id) - post_155_v_start) / 2;
    if (chain_id != current_chain_id)
    {
      throw std::logic_error(fmt::format(
        "Parsed chain ID {} (from v {}), expected to find current chain ID {}",
        chain_id,
        v,
        current_chain_id));
    }

    return rec_id;
  }

  inline eevm::rlp::ByteString encode_optional_address(
    const std::optional<eevm::Address>& address)
  {
    // The encoding of addresses must be either a fixed-length 20-bytes, or
    // the empty list for the null in contract-creation. If treated as a
    // number, any leading 0s would be stripped.
    eevm::rlp::ByteString encoded;
    if (address.has_value())
    {
      constexpr size_t address_length = 20;
      uint8_t address_bytes[address_length] = {};
      intx::be::trunc(address_bytes, *address);
      encoded.insert(
        encoded.end(), std::begin(address_bytes), std::end(address_bytes));
    }
    return encoded;
  }

  inline eevm::Address get_address_from_public_key_asn1(
    const std::vector<uint8_t>& asn1)
  {
    // Check the bytes are prefixed with the ASN.1 type tag we expect,
    // then return raw bytes without type tag prefix.
    if (asn1[0] != MBEDTLS_ASN1_OCTET_STRING)
    {
      throw std::logic_error(fmt::format(
        "Expected ASN.1 key to begin with {}, not {}",
        MBEDTLS_ASN1_OCTET_STRING,
        asn1[0]));
    }

    const std::vector<uint8_t> bytes(asn1.begin() + 1, asn1.end());
    const auto hashed = eevm::keccak_256(bytes);

    // Address is the last 20 bytes of 32-byte hash, so skip first 12
    return eevm::from_big_endian(hashed.data() + 12, 20u);
  }

  struct EthereumTransaction
  {
  protected:
    EthereumTransaction() {}

  public:
    size_t nonce;
    uint256_t gas_price;
    uint256_t gas;
    eevm::rlp::ByteString to;
    uint256_t value;
    eevm::rlp::ByteString data;

    EthereumTransaction(size_t nonce_, const rpcparams::MessageCall& tc)
    {
      nonce = nonce_;
      gas_price = tc.gas_price;
      gas = tc.gas;
      to = encode_optional_address(tc.to);
      value = tc.value;
      data = eevm::to_bytes(tc.data);
    }

    EthereumTransaction(const eevm::rlp::ByteString& encoded)
    {
      auto tup = eevm::rlp::decode<
        size_t,
        uint256_t,
        uint256_t,
        eevm::rlp::ByteString,
        uint256_t,
        eevm::rlp::ByteString>(encoded);

      nonce = std::get<0>(tup);
      gas_price = std::get<1>(tup);
      gas = std::get<2>(tup);
      to = std::get<3>(tup);
      value = std::get<4>(tup);
      data = std::get<5>(tup);
    }

    eevm::rlp::ByteString encode() const
    {
      return eevm::rlp::encode(nonce, gas_price, gas, to, value, data);
    }

    virtual eevm::KeccakHash to_be_signed() const
    {
      return eevm::keccak_256(encode());
    }

    virtual void to_transaction_call(rpcparams::MessageCall& tc) const
    {
      tc.gas_price = gas_price;
      tc.gas = gas;
      if (to.empty())
      {
        tc.to = std::nullopt;
      }
      else
      {
        tc.to = eevm::from_big_endian(to.data(), to.size());
      }
      tc.value = value;
      tc.data = eevm::to_hex_string(data);
    }
  };

  struct EthereumTransactionWithSignature : public EthereumTransaction
  {
    // In tls::RecoverableSignature, r and s are combined in a single fixed-size
    // array. The first 32 bytes contain r, the second 32 contain s.
    static constexpr size_t r_fixed_length = 32u;

    using PointCoord = uint256_t;
    uint8_t v;
    PointCoord r;
    PointCoord s;

    EthereumTransactionWithSignature(
      const EthereumTransaction& tx,
      uint8_t v_,
      const PointCoord& r_,
      const PointCoord& s_) :
      EthereumTransaction(tx)
    {
      v = v_;
      r = r_;
      s = s_;
    }

    EthereumTransactionWithSignature(
      const EthereumTransaction& tx, const tls::RecoverableSignature& sig) :
      EthereumTransaction(tx)
    {
      v = to_ethereum_recovery_id(sig.recovery_id);

      const auto s_data = sig.raw.begin() + r_fixed_length;
      r = eevm::from_big_endian(sig.raw.data(), r_fixed_length);
      s = eevm::from_big_endian(s_data, r_fixed_length);
    }

    EthereumTransactionWithSignature(const eevm::rlp::ByteString& encoded)
    {
      auto tup = eevm::rlp::decode<
        size_t,
        uint256_t,
        uint256_t,
        eevm::rlp::ByteString,
        uint256_t,
        eevm::rlp::ByteString,
        uint8_t,
        PointCoord,
        PointCoord>(encoded);

      nonce = std::get<0>(tup);
      gas_price = std::get<1>(tup);
      gas = std::get<2>(tup);
      to = std::get<3>(tup);
      value = std::get<4>(tup);
      data = std::get<5>(tup);
      v = std::get<6>(tup);
      r = std::get<7>(tup);
      s = std::get<8>(tup);
    }

    eevm::rlp::ByteString encode() const
    {
      return eevm::rlp::encode(nonce, gas_price, gas, to, value, data, v, r, s);
    }

    void to_recoverable_signature(tls::RecoverableSignature& sig) const
    {
      sig.recovery_id = from_ethereum_recovery_id(v);

      const auto s_begin = sig.raw.data() + r_fixed_length;
      eevm::to_big_endian(r, sig.raw.data());
      eevm::to_big_endian(s, s_begin);
    }

    eevm::KeccakHash to_be_signed() const override
    {
      if (is_pre_eip_155(v))
      {
        return EthereumTransaction::to_be_signed();
      }

      // EIP-155 adds (CHAIN_ID, 0, 0) to the data which is hashed, but _only_
      // for signing/recovering. The canonical transaction hash (produced by
      // encode(), used as transaction ID) is unaffected
      return eevm::keccak_256(eevm::rlp::encode(
        nonce, gas_price, gas, to, value, data, current_chain_id, 0, 0));
    }

    void to_transaction_call(rpcparams::MessageCall& tc) const override
    {
      EthereumTransaction::to_transaction_call(tc);

      tls::RecoverableSignature rs;
      to_recoverable_signature(rs);
      const auto tbs = to_be_signed();
      auto pubk =
        tls::PublicKey_k1Bitcoin::recover_key(rs, {tbs.data(), tbs.size()});
      tc.from = get_address_from_public_key_asn1(pubk.public_key_asn1());
    }
  };

  inline EthereumTransactionWithSignature sign_transaction(
    tls::KeyPair_k1Bitcoin& kp, const EthereumTransaction& tx)
  {
    const auto tbs = tx.to_be_signed();
    const auto signature = kp.sign_recoverable_hashed({tbs.data(), tbs.size()});
    return EthereumTransactionWithSignature(tx, signature);
  }
} // namespace evm4ccf
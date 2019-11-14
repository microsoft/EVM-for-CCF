// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <msgpack-c/msgpack.hpp>

// To instantiate the kv map types above, all keys and values must be
// convertible to msgpack
namespace msgpack
{
  MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS)
  {
    namespace adaptor
    {
      // msgpack conversion for uint256_t
      template <>
      struct convert<uint256_t>
      {
        msgpack::object const& operator()(
          msgpack::object const& o, uint256_t& v) const
        {
          const std::vector<uint8_t> vec =
            o.via.array.ptr[0].as<std::vector<uint8_t>>();
          v = eevm::from_big_endian(vec.data(), vec.size());

          return o;
        }
      };

      template <>
      struct pack<uint256_t>
      {
        template <typename Stream>
        packer<Stream>& operator()(
          msgpack::packer<Stream>& o, uint256_t const& v) const
        {
          std::vector<uint8_t> big_end_val(0x20); // size of 256 bits in bytes
          eevm::to_big_endian(v, big_end_val.data());
          o.pack_array(1);
          o.pack(big_end_val);
          return o;
        }
      };

      // msgpack conversion for eevm::LogEntry
      template <>
      struct convert<eevm::LogEntry>
      {
        msgpack::object const& operator()(
          msgpack::object const& o, eevm::LogEntry& v) const
        {
          auto addr = o.via.array.ptr[0].as<eevm::Address>();
          auto data = o.via.array.ptr[1].as<eevm::log::Data>();
          auto topics = o.via.array.ptr[2].as<std::vector<eevm::log::Topic>>();
          v = {addr, data, topics};

          return o;
        }
      };

      template <>
      struct pack<eevm::LogEntry>
      {
        template <typename Stream>
        packer<Stream>& operator()(
          msgpack::packer<Stream>& o, eevm::LogEntry const& v) const
        {
          o.pack_array(3);
          o.pack(v.address);
          o.pack(v.data);
          o.pack(v.topics);
          return o;
        }
      };

      // msgpack conversion for evm4ccf::TxResult
      template <>
      struct convert<evm4ccf::TxResult>
      {
        msgpack::object const& operator()(
          msgpack::object const& o, evm4ccf::TxResult& v) const
        {
          auto addr = o.via.array.ptr[0].as<eevm::Address>();
          if (addr != 0)
          {
            v.contract_address = addr;
          }
          v.logs = o.via.array.ptr[1].as<std::vector<eevm::LogEntry>>();

          return o;
        }
      };

      template <>
      struct pack<evm4ccf::TxResult>
      {
        template <typename Stream>
        packer<Stream>& operator()(
          msgpack::packer<Stream>& o, evm4ccf::TxResult const& v) const
        {
          o.pack_array(2);
          o.pack(v.contract_address.value_or(0x0));
          o.pack(v.logs);
          return o;
        }
      };

      // msgpack conversion for evm4ccf::BlockHeader
      template <>
      struct convert<evm4ccf::BlockHeader>
      {
        msgpack::object const& operator()(
          msgpack::object const& o, evm4ccf::BlockHeader& v) const
        {
          v.number = o.via.array.ptr[0].as<uint64_t>();
          v.difficulty = o.via.array.ptr[1].as<uint64_t>();
          v.gas_limit = o.via.array.ptr[2].as<uint64_t>();
          v.gas_used = o.via.array.ptr[3].as<uint64_t>();
          v.timestamp = o.via.array.ptr[4].as<uint64_t>();
          v.miner = o.via.array.ptr[5].as<decltype(v.miner)>();
          v.block_hash = o.via.array.ptr[6].as<decltype(v.block_hash)>();

          return o;
        }
      };

      template <>
      struct pack<evm4ccf::BlockHeader>
      {
        template <typename Stream>
        packer<Stream>& operator()(
          msgpack::packer<Stream>& o, evm4ccf::BlockHeader const& v) const
        {
          o.pack_array(7);
          o.pack(v.number);
          o.pack(v.difficulty);
          o.pack(v.gas_limit);
          o.pack(v.gas_used);
          o.pack(v.timestamp);
          o.pack(v.miner);
          o.pack(v.block_hash);
          return o;
        }
      };
    } // namespace adaptor
  } // namespace msgpack
} // namespace msgpack

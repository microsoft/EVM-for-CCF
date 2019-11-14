// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "../src/app/tables.h"
#include "kv/kvserialiser.h"

#include <doctest/doctest.h>
#include <random>

using namespace ccf;
using namespace evm4ccf;

#ifdef USE_NLJSON_KV_SERIALISER
using KvWriter = kv::JsonWriter;
using KvReader = kv::JsonReader;
#else
using KvWriter = kv::MsgPackWriter;
using KvReader = kv::MsgPackReader;
#endif

// Template
template <typename... Ts>
void write_all(KvWriter& writer, Ts&&... ts);

// Terminating final case
template <>
inline void write_all(KvWriter& writer)
{}

// General case - recurse
template <typename T, typename... Ts>
inline void write_all(KvWriter& writer, T&& t, Ts&&... ts)
{
  writer.append(t);
  write_all(writer, std::forward<Ts>(ts)...);
}

// Template
template <typename... Ts>
void read_and_compare_all(KvReader& reader, Ts&&... ts);

// Terminating final case
template <>
inline void read_and_compare_all(KvReader& reader)
{
  REQUIRE(reader.is_eos());
}

// General case - recurse
template <typename T, typename... Ts>
inline void read_and_compare_all(KvReader& reader, T&& t, Ts&&... ts)
{
  REQUIRE(reader.read_next<std::decay_t<T>>() == t);
  read_and_compare_all(reader, std::forward<Ts>(ts)...);
}

// Require that the given arguments survive a roundtrip. Write all, read all,
// check each matches
template <typename... Ts>
void require_roundtrip(Ts&&... ts)
{
  KvWriter writer;

  write_all(writer, std::forward<Ts>(ts)...);

  const auto raw_data = writer.get_raw_data();
  KvReader reader(raw_data.data(), raw_data.size());

  read_and_compare_all(reader, std::forward<Ts>(ts)...);
}

//
// Generators for creating plausible random contents
//
template <typename T>
T make_rand()
{
  return (T)rand();
}

std::vector<uint8_t> rand_bytes(size_t max_len = 100)
{
  std::vector<uint8_t> result(rand() % max_len);
  if (result.empty()) // Don't allow empty bytes
    result.resize(1);
  for (size_t i = 0; i < result.size(); ++i)
    result[i] = (uint8_t)rand();
  return result;
}

template <>
std::vector<uint8_t> make_rand<std::vector<uint8_t>>()
{
  return rand_bytes();
}

template <>
uint256_t make_rand<uint256_t>()
{
  const auto bytes = rand_bytes(32);
  return eevm::from_big_endian(bytes.data(), bytes.size());
}

template <>
eevm::LogEntry make_rand<eevm::LogEntry>()
{
  std::vector<eevm::log::Topic> topics(rand() % 4);
  for (size_t i = 0; i < topics.size(); ++i)
    topics[i] = make_rand<eevm::log::Topic>();
  return eevm::LogEntry{make_rand<decltype(eevm::LogEntry::address)>(),
                        make_rand<decltype(eevm::LogEntry::data)>(),
                        topics};
}

template <>
evm4ccf::TxResult make_rand<evm4ccf::TxResult>()
{
  std::vector<eevm::LogEntry> logs(rand() % 10);
  for (size_t i = 0; i < logs.size(); ++i)
    logs[i] = make_rand<eevm::LogEntry>();
  return evm4ccf::TxResult{make_rand<decltype(eevm::LogEntry::address)>(),
                           logs};
}

template <>
evm4ccf::BlockHeader make_rand<evm4ccf::BlockHeader>()
{
  return evm4ccf::BlockHeader{
    make_rand<decltype(evm4ccf::BlockHeader::number)>(),
    make_rand<decltype(evm4ccf::BlockHeader::difficulty)>(),
    make_rand<decltype(evm4ccf::BlockHeader::gas_limit)>(),
    make_rand<decltype(evm4ccf::BlockHeader::gas_used)>(),
    make_rand<decltype(evm4ccf::BlockHeader::timestamp)>(),
    make_rand<decltype(evm4ccf::BlockHeader::miner)>(),
    make_rand<decltype(evm4ccf::BlockHeader::block_hash)>()};
}

using namespace intx;
const uint256_t address = 0x4af4dcE351A4747B5b33Fcf66202612736401f95_u256;

TEST_CASE("Hex-string conversion" * doctest::test_suite("conversions"))
{
  using A32 = std::array<uint8_t, 32u>;
  {
    A32 arr;
    for (size_t i = 0; i < arr.size(); ++i)
    {
      arr[i] = (uint8_t)i;
    }
    const auto s = eevm::to_hex_string(arr);
    A32 arr2;
    array_from_hex_string(arr2, s);

    REQUIRE(arr == arr2);
  }

  {
    const auto s =
      "0x9c93e6106f4b66c515d2e491e58799a8df69e95ad1ecf9263465d200203583e9";
    A32 arr;
    array_from_hex_string(arr, s);
    const auto s2 = eevm::to_hex_string(arr);

    REQUIRE(s == s2);
  }
}

TEST_CASE("Empty" * doctest::test_suite("conversions"))
{
  require_roundtrip();
}

TEST_CASE("uint256_t" * doctest::test_suite("conversions"))
{
  const uint256_t a = 0;
  const uint256_t b = 1;
  const uint256_t c = 0x123412341234123412341234123412341234_u256;
  const auto d = address;

  require_roundtrip(a, b, c, d);
  require_roundtrip(make_rand<uint256_t>());
}

TEST_CASE("eevm::LogEntry" * doctest::test_suite("conversions"))
{
  const eevm::LogEntry a{};
  const eevm::LogEntry b{0x1, {0x1, 0x2, 0x3, 0x4, 0x5, 0x6}, {0xaabb}};
  const eevm::LogEntry c{address, {}, {0xa, 0xb, 0xc, 0xd}};

  require_roundtrip(a, b, c);
  require_roundtrip(make_rand<eevm::LogEntry>());
}

TEST_CASE("evm4ccf::TxResult" * doctest::test_suite("conversions"))
{
  const evm4ccf::TxResult a{};
  const evm4ccf::TxResult b{0x1,
                            {{0x1, {0x1, 0x2, 0x3, 0x4, 0x5, 0x6}, {0xaabb}}}};
  const evm4ccf::TxResult c{address,
                            {
                              {0x1, {0x1, 0x2, 0x3, 0x4, 0x5, 0x6}, {0xaabb}},
                              {address,
                               {0x0, 0x0, 0xff, 0xfe, 0xef, 0xee, 0xaa},
                               {0xaabb, 0xab, 0xcd, 0xdc}},
                            }};

  require_roundtrip(a, b, c);
  require_roundtrip(make_rand<evm4ccf::TxResult>());
}

TEST_CASE("evm4ccf::BlockHeader" * doctest::test_suite("conversions"))
{
  const evm4ccf::BlockHeader a{};
  const evm4ccf::BlockHeader b{0, 1, 2, 3, 4};
  const evm4ccf::BlockHeader c{0x55, 0x44, 0x33, 0x22, 0x11};

  require_roundtrip(a, b, c);
  require_roundtrip(make_rand<evm4ccf::BlockHeader>());
}

TEST_CASE("mixed random" * doctest::test_suite("conversions"))
{
  require_roundtrip(
    make_rand<uint256_t>(),
    make_rand<eevm::LogEntry>(),
    make_rand<evm4ccf::TxResult>(),
    make_rand<evm4ccf::BlockHeader>());

  require_roundtrip(
    make_rand<evm4ccf::BlockHeader>(),
    make_rand<uint256_t>(),
    make_rand<uint256_t>(),
    make_rand<evm4ccf::TxResult>(),
    make_rand<eevm::LogEntry>(),
    make_rand<evm4ccf::TxResult>(),
    make_rand<eevm::LogEntry>(),
    make_rand<evm4ccf::BlockHeader>());
}

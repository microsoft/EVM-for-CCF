// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "ds/logger.h"
#include "enclave/appinterface.h"
#include "rpc_types.h"
#include "shared.h"

#include <doctest/doctest.h>
#include <set>
#include <vector>

// Utils for filtering logs. Should be moved to include if we ever need process
// events in C++
namespace evm4ccf
{
  namespace logfilter
  {
    struct Filter
    {
      std::set<eevm::Address> addresses;
      std::vector<eevm::log::Topic> topics;
    };

    inline bool matches_filter(const Filter& filter, const eevm::LogEntry& log)
    {
      // If filter defines some addresses but not this, it doesn't match
      if (
        !filter.addresses.empty() &&
        filter.addresses.find(log.address) == filter.addresses.end())
        return false;

      for (size_t i = 0; i < filter.topics.size(); ++i)
      {
        if (i >= log.topics.size() || filter.topics[i] != log.topics[i])
        {
          return false;
        }
      }

      return true;
    }

    inline void get_matching_log_entries(
      const Filter& filter,
      const std::vector<eevm::LogEntry>& logs,
      std::vector<eevm::LogEntry>& matches)
    {
      for (const auto& entry : logs)
      {
        if (matches_filter(filter, entry))
        {
          matches.emplace_back(entry);
        }
      }
    }
  } // namespace logfilter
} // namespace evm4ccf

using namespace ccf;
using namespace evm4ccf;

using LogEntries = std::vector<eevm::LogEntry>;
using LogMap = std::map<TxHash, LogEntries>;
using Match = std::pair<TxHash, eevm::LogEntry>;
using Matches = std::vector<Match>;

Matches get_matches(const LogMap& lm, const logfilter::Filter& f)
{
  Matches matches;

  for (const auto& p : lm)
  {
    for (const auto& log_entry : p.second)
    {
      if (logfilter::matches_filter(f, log_entry))
      {
        matches.emplace_back(std::make_pair(p.first, log_entry));
      }
    }
  }

  return matches;
};

TEST_CASE("Filter0" * doctest::test_suite("filter"))
{
  // Tests the filter matching logic in isolation
  using namespace evm4ccf::logfilter;
  using namespace eevm;

  const TxHash tx0{0xbeef};
  const TxHash tx1{0xcafe};
  const TxHash tx2{0xfeed};
  const TxHash logless{0xdead};

  const Address address_a = eevm::to_uint256("0xa");
  const Address address_b = eevm::to_uint256("0xbb");
  const Address address_c = eevm::to_uint256("0xccc");

  const log::Topic topic0 = eevm::to_uint256("0x0");
  const log::Topic topic1 = eevm::to_uint256("0x10");
  const log::Topic topic2 = eevm::to_uint256("0x200");
  const log::Topic topic3 = eevm::to_uint256("0x3000");

  const log::Data d0 = to_bytes("0x0123456789abcdef");
  const log::Data d1 = to_bytes("0xffaffaffaffa");
  const log::Data d2 = to_bytes(
    "0xabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabba"
    "abbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabba");
  const log::Data d3 = to_bytes("0x1337");
  const log::Data d4 = to_bytes("0x42");

  // Create some log entries manually
  LogMap transaction_logs;
  {
    transaction_logs.emplace(
      tx0,
      LogEntries{{address_a, d0, {}},
                 {address_a, d1, {topic0}},
                 {address_a, d2, {topic0, topic1}},
                 {address_a, d3, {topic0, topic1, topic2}},
                 {address_a, d4, {topic0, topic1, topic2, topic3}}});

    transaction_logs.emplace(
      tx1,
      LogEntries{{address_a, d0, {topic2}},
                 {address_b, d0, {topic2}},
                 {address_b, d0, {topic2}},
                 {address_b, d1, {topic2}},
                 {address_c, d1, {topic2}}});

    transaction_logs.emplace(
      tx2,
      LogEntries{{address_c, d4, {topic2, topic2, topic2}},
                 {address_a, d1, {topic0, topic1}},
                 {address_a, d3, {topic1, topic0}},
                 {address_a, d0, {topic0, topic1, topic2}},
                 {address_c, d4, {topic2, topic2}},
                 {address_c, d4, {topic2, topic2, topic1}},
                 {address_b, d0, {topic2}},
                 {address_a, d4, {topic2}},
                 {address_b, d0, {topic0, topic1, topic2}},
                 {address_a, d2, {topic0, topic2, topic1}}});

    transaction_logs.emplace(logless, LogEntries{});
  }

  // Util functions for counting matches
  auto get_tx_count = [](const Matches& ms, const TxHash& n) {
    return std::count_if(
      ms.begin(), ms.end(), [&n](const auto& p) { return p.first == n; });
  };

  auto get_address_count = [](const Matches& ms, const Address& a) {
    return std::count_if(ms.begin(), ms.end(), [&a](const auto& p) {
      return p.second.address == a;
    });
  };

  auto get_data_count = [](const Matches& ms, const log::Data& d) {
    return std::count_if(
      ms.begin(), ms.end(), [&d](const auto& p) { return p.second.data == d; });
  };

  // Build some filters, check they return the expected log entries
  SUBCASE("filter can get everything")
  {
    Filter filter;
    filter.addresses = {};
    filter.topics = {};
    auto matches = get_matches(transaction_logs, filter);

    CHECK(matches.size() == 20);
    CHECK(0 == get_tx_count(matches, logless));
  }

  SUBCASE("filter can get single result")
  {
    Filter filter;
    filter.addresses = {address_a};
    filter.topics = {topic0, topic1, topic2, topic3};
    auto matches = get_matches(transaction_logs, filter);

    CHECK(matches.size() == 1);
    const auto& p = matches[0];
    CHECK(p.first == tx0);
    const auto& e = p.second;
    CHECK(e.address == address_a);
    CHECK(e.topics == filter.topics);
    CHECK(e.data == d4);
    CHECK(0 == get_tx_count(matches, logless));
  }

  SUBCASE("filters can get multiple results")
  {
    Filter filter;
    filter.addresses = {address_a};
    filter.topics = {topic0, topic1, topic2};
    auto matches = get_matches(transaction_logs, filter);

    CHECK(matches.size() == 3);
    for (const auto& p : matches)
    {
      const auto& e = p.second;
      CHECK(e.address == address_a);
      CHECK(e.topics[0] == topic0);
      CHECK(e.topics[1] == topic1);
      CHECK(e.topics[2] == topic2);
    }
    CHECK(2 == get_tx_count(matches, tx0));
    CHECK(1 == get_tx_count(matches, tx2));
    CHECK(0 == get_tx_count(matches, logless));
  }

  SUBCASE("entries can repeat, within and across transactions")
  {
    Filter filter;
    filter.addresses = {address_b};
    filter.topics = {topic2};
    auto matches = get_matches(transaction_logs, filter);

    CHECK(matches.size() == 4);
    for (const auto& p : matches)
    {
      const auto& e = p.second;
      CHECK(e.address == address_b);
      CHECK(e.topics[0] == topic2);
    }
    CHECK(3 == get_tx_count(matches, tx1));
    CHECK(1 == get_tx_count(matches, tx2));
    CHECK(3 == get_data_count(matches, d0));
    CHECK(1 == get_data_count(matches, d1));
    CHECK(0 == get_tx_count(matches, logless));
  }

  SUBCASE("filter can omit address")
  {
    Filter filter;
    filter.addresses = {};
    filter.topics = {topic0, topic1, topic2};
    auto matches = get_matches(transaction_logs, filter);

    CHECK(matches.size() == 4);
    for (const auto& p : matches)
    {
      const auto& e = p.second;
      CHECK(e.topics[0] == topic0);
      CHECK(e.topics[1] == topic1);
      CHECK(e.topics[2] == topic2);
    }
    CHECK(2 == get_tx_count(matches, tx0));
    CHECK(2 == get_tx_count(matches, tx2));
    CHECK(3 == get_address_count(matches, address_a));
    CHECK(1 == get_address_count(matches, address_b));
    CHECK(2 == get_data_count(matches, d0));
    CHECK(1 == get_data_count(matches, d3));
    CHECK(1 == get_data_count(matches, d4));
    CHECK(0 == get_tx_count(matches, logless));
  }

  SUBCASE("filter can omit topics")
  {
    Filter filter;
    filter.addresses = {address_b, address_c};
    filter.topics = {};
    auto matches = get_matches(transaction_logs, filter);

    CHECK(matches.size() == 9);
    CHECK(0 == get_tx_count(matches, tx0));
    CHECK(4 == get_tx_count(matches, tx1));
    CHECK(5 == get_tx_count(matches, tx2));
    CHECK(0 == get_address_count(matches, address_a));
    CHECK(5 == get_address_count(matches, address_b));
    CHECK(4 == get_address_count(matches, address_c));
    CHECK(4 == get_data_count(matches, d0));
    CHECK(2 == get_data_count(matches, d1));
    CHECK(0 == get_data_count(matches, d2));
    CHECK(0 == get_data_count(matches, d3));
    CHECK(3 == get_data_count(matches, d4));
    CHECK(0 == get_tx_count(matches, logless));
  }
}

TEST_CASE("TransactionLogs0" * doctest::test_suite("logs"))
{
  NetworkTables nwt;
  StubNotifier stubn;
  Store& tables = *nwt.tables;
  auto cert = setup_tables(tables);
  Ethereum frontend = ccfapp::get_rpc_handler(nwt, stubn);
  jsonrpc::SeqNo sn = 0;
  eevm::Address created;

  SUBCASE("log0")
  {
    // Store ED in memory, then call log0 to write this to the log
    const auto code = "0x60ED60005260206000A0";

    // Create a contract with simple logging code
    {
      created = deploy_contract(code, frontend, cert);
    }

    // Send a transaction that calls this code
    TxHash tx_hash;
    {
      auto in = ethrpc::SendTransaction::make(sn++);
      in.params.call_data.from = created;
      in.params.call_data.to = created;
      in.params.call_data.data = "0x";
      ethrpc::SendTransaction::Out out = do_rpc(frontend, cert, in);
      tx_hash = out.result;
    }

    // Get the produced logs from the TxReceipt
    std::vector<eevm::LogEntry> logs;
    {
      auto in = ethrpc::GetTransactionReceipt::make(sn++);
      in.params.tx_hash = tx_hash;
      ethrpc::GetTransactionReceipt::Out out = do_rpc(frontend, cert, in);
      REQUIRE(out.result.has_value());
      logs = out.result->logs;
    }

    // Check the logs match what we expect
    {
      REQUIRE(logs.size() == 1);
      const auto& log_entry = logs[0];
      CHECK(
        eevm::from_big_endian(log_entry.data.data(), log_entry.data.size()) ==
        0xED);
      CHECK(log_entry.topics.empty());
    }
  }

  // Store 0x1234ABCD in memory, then call log2 to write these 4 bytes to
  // the log with topics [0xE, 0xF]
  const auto code = "0x631234ABCD600052600F600E6004601CA2";
  SUBCASE("log2")
  {
    // Deploy
    {
      created = deploy_contract(code, frontend, cert);
    }

    {
      // Call code
      auto in = ethrpc::SendTransaction::make(sn++);
      in.params.call_data.from = created;
      in.params.call_data.to = created;
      in.params.call_data.data =
        "0xFFAAFFAAFFAA"; // Input data is provided, but unused
      ethrpc::SendTransaction::Out out = do_rpc(frontend, cert, in);
      const auto tx_hash = out.result;

      // Get logs
      auto tx_in = ethrpc::GetTransactionReceipt::make(sn++);
      tx_in.params.tx_hash = tx_hash;
      ethrpc::GetTransactionReceipt::Out tx_out = do_rpc(frontend, cert, tx_in);
      REQUIRE(tx_out.result.has_value());
      const auto logs = tx_out.result->logs;

      // Check log contents
      REQUIRE(logs.size() == 1);
      const auto& log_entry = logs[0];
      CHECK(log_entry.data.size() == 4);
      CHECK(
        eevm::from_big_endian(log_entry.data.data(), log_entry.data.size()) ==
        0x1234ABCD);
      CHECK(log_entry.topics.size() == 2);
      CHECK(log_entry.topics[0] == 0xE);
      CHECK(log_entry.topics[1] == 0xF);
    }
  }

  SUBCASE("similar transactions produce distinct logs")
  {
    // Deploy
    {
      created = deploy_contract(code, frontend, cert);
    }

    std::vector<eevm::LogEntry> all_logs;
    constexpr auto n = 5;
    for (auto i = 0; i < n; ++i)
    {
      // Send transactions, identical other than sequence number
      auto in = ethrpc::SendTransaction::make(sn++);
      in.params.call_data.from = created;
      in.params.call_data.to = created;
      ethrpc::SendTransaction::Out out = do_rpc(frontend, cert, in);
      const auto tx_hash = out.result;

      // Get logs
      auto tx_in = ethrpc::GetTransactionReceipt::make(sn++);
      tx_in.params.tx_hash = tx_hash;
      ethrpc::GetTransactionReceipt::Out tx_out = do_rpc(frontend, cert, tx_in);
      REQUIRE(tx_out.result.has_value());
      all_logs.insert(
        all_logs.end(), tx_out.result->logs.begin(), tx_out.result->logs.end());
    }

    CHECK(all_logs.size() == n);
    for (const auto& log_entry : all_logs)
    {
      CHECK(log_entry.data.size() == 4);
      CHECK(
        eevm::from_big_endian(log_entry.data.data(), log_entry.data.size()) ==
        0x1234ABCD);
      CHECK(log_entry.topics.size() == 2);
      CHECK(log_entry.topics[0] == 0xE);
      CHECK(log_entry.topics[1] == 0xF);
    }
  }
}

TEST_CASE("TransactionLogs1" * doctest::test_suite("logs"))
{
  using namespace evm4ccf::logfilter;

  // Test filter installation, uninstallation, log-retrieval
  NetworkTables nwt;
  StubNotifier stubn;
  Store& tables = *nwt.tables;
  auto cert = setup_tables(tables);
  Ethereum frontend = ccfapp::get_rpc_handler(nwt, stubn);
  jsonrpc::SeqNo sn = 0;

  LogMap tx_logs;

  TestAccount owner(frontend, tables);
  TestAccount sender_a(frontend, tables);
  TestAccount sender_b(frontend, tables);

  auto retrieve_logs = [&](const TxHash& tx_hash) {
    auto in = ethrpc::GetTransactionReceipt::make(sn++);
    in.params.tx_hash = tx_hash;
    ethrpc::GetTransactionReceipt::Out out = do_rpc(frontend, cert, in);
    REQUIRE(out.result.has_value());

    REQUIRE(
      tx_logs.find(tx_hash) ==
      tx_logs.end()); // Shouldn't get duplicate tx hashes

    tx_logs[tx_hash] = out.result->logs;
  };

  // Deploy a contract compiled from Solidity, that does logging through events
  // See Events.sol for source
  const auto compiled = read_bytecode("Events");

  // Call constructor
  const auto constructor = compiled.deploy;
  const auto deployed_code = compiled.runtime;
  const auto a = eevm::keccak_256("Name(address)");
  const auto topic_eventhash_name = eevm::from_big_endian(a.data(), a.size());
  const auto b = eevm::keccak_256("Interesting(uint256,uint256,uint256)");
  const auto topic_eventhash_interesting =
    eevm::from_big_endian(b.data(), b.size());
  TxHash deploy_hash;
  const auto contract = owner.deploy_contract(constructor, &deploy_hash);
  REQUIRE(owner.get_code(contract) == deployed_code);

  retrieve_logs(deploy_hash);

  // Make helper functions to call the contract's public methods
  auto name_self = [&contract, &retrieve_logs](TestAccount& ta) {
    std::stringstream ss;
    ss << "01984892" << std::hex << std::setfill('0') << std::setw(64)
       << ta.address;
    const auto tx_hash = ta.contract_transact(contract, ss.str());
    retrieve_logs(tx_hash);
  };

  auto emit_event = [&contract, &retrieve_logs](
                      TestAccount& ta,
                      const uint256_t& topic,
                      const uint256_t& b,
                      const uint256_t& c) {
    std::stringstream ss;
    ss << "789ab20b";
    ss << std::hex << std::setfill('0') << std::setw(64) << topic;
    ss << std::hex << std::setfill('0') << std::setw(64) << b;
    ss << std::hex << std::setfill('0') << std::setw(64) << c;
    const auto tx_hash = ta.contract_transact(contract, ss.str());
    retrieve_logs(tx_hash);
  };

  auto get_event_count =
    [](const Matches& ms, const uint256_t& a, const uint256_t& b) {
      eevm::log::Data expected;
      expected.resize(64u);
      eevm::to_big_endian(a, expected.data());
      eevm::to_big_endian(b, expected.data() + 32u);
      return std::count_if(ms.begin(), ms.end(), [&expected](const auto& p) {
        return p.second.data == expected;
      });
    };

  SUBCASE("transaction logs can be filtered")
  {
    Filter filter_everything;
    {
      INFO("constructor produced one event");
      auto all_logs = get_matches(tx_logs, filter_everything);
      CHECK(all_logs.size() == 1);
      const auto& p = all_logs[0];
      const auto& entry = p.second;
      CHECK(entry.address == contract);
      CHECK(entry.topics.empty());
      CHECK(
        0xdeadbeef ==
        eevm::from_big_endian(entry.data.data(), entry.data.size()));
    }

    // Send transactions
    name_self(owner);
    name_self(sender_a);
    emit_event(sender_a, 0xbeef, 0xaaaa, 0xaaaa);
    emit_event(sender_b, 0xbeef, 0x1111, 0x1111);
    emit_event(sender_b, 0xfeeb, 0x2211, 0xaaaa);
    emit_event(sender_b, 0x0, 0x1111, 0x1111);
    emit_event(sender_b, 0xbeef, 0xcafe, 0xfeed);
    emit_event(sender_b, 0xbeef, 0xcafe, 0xfeed);
    emit_event(sender_a, 0xbeef, 0xcafe, 0xfeed);

    auto all_logs = get_matches(tx_logs, filter_everything);

    SUBCASE("only the contract has created logs")
    {
      Filter contract_only;
      contract_only.addresses = {contract};
      auto contract_logs = get_matches(tx_logs, contract_only);
      CHECK(all_logs.size() == contract_logs.size());
      for (const auto& log_pair : all_logs)
      {
        CHECK(log_pair.second.address == contract);
      }
    }

    SUBCASE("logs can be filtered by event")
    {
      Filter name_events_only;
      name_events_only.topics = {topic_eventhash_name};
      auto name_event_logs = get_matches(tx_logs, name_events_only);
      CHECK(name_event_logs.size() == 2);
      std::set<eevm::Address> logged_names;
      for (const auto& log_pair : name_event_logs)
        logged_names.insert(eevm::from_big_endian(
          log_pair.second.data.data(), log_pair.second.data.size()));
      CHECK(logged_names.find(owner.address) != logged_names.end());
      CHECK(logged_names.find(sender_a.address) != logged_names.end());
    }

    SUBCASE("logs can be filtered by indexed topic")
    {
      Filter filter_beef;
      filter_beef.topics = {topic_eventhash_interesting, 0xbeef};
      auto beef_logs = get_matches(tx_logs, filter_beef);
      CHECK(5 == beef_logs.size());
      CHECK(1 == get_event_count(beef_logs, 0xaaaa, 0xaaaa));
      CHECK(1 == get_event_count(beef_logs, 0x1111, 0x1111));
      CHECK(3 == get_event_count(beef_logs, 0xcafe, 0xfeed));
    }
  }
}

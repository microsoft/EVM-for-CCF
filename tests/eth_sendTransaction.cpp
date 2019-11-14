// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "ds/logger.h"
#include "enclave/appinterface.h"
#include "shared.h"

#include <doctest/doctest.h>

using namespace ccf;
using namespace evm4ccf;

template <typename T>
nlohmann::json json_with(
  const nlohmann::json& j, const std::string& field, T&& v)
{
  nlohmann::json result = j;
  result[field] = v;
  return result;
}

nlohmann::json json_without(const nlohmann::json& j, const std::string& field)
{
  nlohmann::json result = j;
  result.erase(field);
  return result;
}

TEST_CASE("JSON format" * doctest::test_suite("transactions"))
{
  const nlohmann::json basic_request = {
    {"from", "0xb60E8dD61C5d32be8058BB8eb970870F07233155"},
    {"to", "0xd46E8dD67C5d32be8058Bb8Eb970870F07244567"},
    {"gas", "0x76c0"},
    {"gasPrice", "0x9184e72a000"},
    {"value", "0x9184e72a"},
    {"data",
     "0xd46e8dd67c5d32be8d46e8dd67c5d32be8058bb8eb970870f072445675058bb8eb97087"
     "0f072445675"},
  };

  {
    INFO("Basic roundtrip");
    const auto tc = basic_request.get<rpcparams::MessageCall>();
    const nlohmann::json converted = tc;

    const auto target = std::string("Target: ") + basic_request.dump();
    INFO(target);
    const auto actual = std::string("Actual: ") + converted.dump();
    INFO(actual);

    CHECK(basic_request == converted);
  }

  // to
  {
    const auto j = json_without(basic_request, "to");
    const auto tc = j.get<rpcparams::MessageCall>();
    CHECK(!tc.to.has_value());
  }

  {
    const auto j = json_with(basic_request, "to", nullptr);
    const auto tc = j.get<rpcparams::MessageCall>();
    CHECK(!tc.to.has_value());
  }

  {
    const auto j = json_with(basic_request, "to", "");
    const auto tc = j.get<rpcparams::MessageCall>();
    CHECK(!tc.to.has_value());
  }

  {
    const auto j = json_with(basic_request, "to", "0x0");
    const auto tc = j.get<rpcparams::MessageCall>();
    CHECK(tc.to.has_value());
    if (tc.to.has_value())
    {
      CHECK(tc.to.value() == 0);
    }
  }

  {
    const auto j = json_with(basic_request, "to", "0x42");
    const auto tc = j.get<rpcparams::MessageCall>();
    CHECK(tc.to.has_value());
    if (tc.to.has_value())
    {
      CHECK(tc.to.value() == 0x42);
    }
  }

  // gas
  const auto default_gas = rpcparams::MessageCall().gas;
  {
    const auto j = json_without(basic_request, "gas");
    const auto tc = j.get<rpcparams::MessageCall>();
    CHECK(tc.gas == default_gas);
  }

  {
    const auto j = json_with(basic_request, "gas", nullptr);
    const auto tc = j.get<rpcparams::MessageCall>();
    CHECK(tc.gas == default_gas);
  }

  {
    const auto j = json_with(basic_request, "gas", "");
    const auto tc = j.get<rpcparams::MessageCall>();
    CHECK(tc.gas == default_gas);
  }

  {
    const auto j = json_with(basic_request, "gas", "0x42");
    const auto tc = j.get<rpcparams::MessageCall>();
    CHECK(tc.gas == 0x42);
  }

  // gas_price
  const auto default_gas_price = rpcparams::MessageCall().gas_price;
  {
    const auto j = json_without(basic_request, "gasPrice");
    const auto tc = j.get<rpcparams::MessageCall>();
    CHECK(tc.gas_price == default_gas_price);
  }

  {
    const auto j = json_with(basic_request, "gasPrice", nullptr);
    const auto tc = j.get<rpcparams::MessageCall>();
    CHECK(tc.gas_price == default_gas_price);
  }

  {
    const auto j = json_with(basic_request, "gasPrice", "");
    const auto tc = j.get<rpcparams::MessageCall>();
    CHECK(tc.gas_price == default_gas_price);
  }

  {
    const auto j = json_with(basic_request, "gasPrice", "0x42");
    const auto tc = j.get<rpcparams::MessageCall>();
    CHECK(tc.gas_price == 0x42);
  }

  // value
  const auto default_value = rpcparams::MessageCall().value;
  {
    const auto j = json_without(basic_request, "value");
    const auto tc = j.get<rpcparams::MessageCall>();
    CHECK(tc.value == default_value);
  }

  {
    const auto j = json_with(basic_request, "value", nullptr);
    const auto tc = j.get<rpcparams::MessageCall>();
    CHECK(tc.value == default_value);
  }

  {
    const auto j = json_with(basic_request, "value", "");
    const auto tc = j.get<rpcparams::MessageCall>();
    CHECK(tc.value == default_value);
  }

  {
    const auto j = json_with(basic_request, "value", "0x42");
    const auto tc = j.get<rpcparams::MessageCall>();
    CHECK(tc.value == 0x42);
  }
}

TEST_CASE("SendTransaction0" * doctest::test_suite("transactions"))
{
  NetworkTables nwt;
  StubNotifier stubn;
  Store& tables = *nwt.tables;
  auto cert = setup_tables(tables);
  Ethereum frontend = ccfapp::get_rpc_handler(nwt, stubn);
  jsonrpc::SeqNo sn = 0;

  const auto compiled = read_bytecode("Call2");

  // Create a contract by transaction
  TxHash deploy_tx_hash;
  {
    auto in = ethrpc::SendTransaction::make(sn++);
    in.params.call_data.data = compiled.deploy;
    const ethrpc::SendTransaction::Out out = do_rpc(frontend, cert, in);
    deploy_tx_hash = out.result;
  }

  // Get contract address from receipt
  eevm::Address deployed_address;
  {
    auto in = ethrpc::GetTransactionReceipt::make(sn++);
    in.params.tx_hash = deploy_tx_hash;
    ethrpc::GetTransactionReceipt::Out out =
      do_rpc(frontend, cert, in).get<ethrpc::GetTransactionReceipt::Out>();
    REQUIRE(out.result.has_value());
    REQUIRE(out.result->contract_address.has_value());
    deployed_address = out.result->contract_address.value();
  }

  // Check the account is correctly created
  {
    auto in = ethrpc::GetCode::make(sn++);
    in.params.address = deployed_address;
    ethrpc::GetCode::Out out = do_rpc(frontend, cert, in);
    REQUIRE(out.result == compiled.runtime);
  }

  const auto mul_100 = abi_append(compiled.hashes["mul(uint256)"], 100);

  // Make read-only Call to deployed contract
  {
    auto in = ethrpc::Call::make(sn++);
    in.params.call_data.to = deployed_address;
    in.params.call_data.data = mul_100;
    ethrpc::Call::Out out = do_rpc(frontend, cert, in);
    uint256_t res = get_result_value(out);
    REQUIRE(res == 4200);
  }

  // Attempting to do the same with SendTransaction results in transaction hash
  // - output is lost
  {
    auto in = ethrpc::SendTransaction::make(sn++);
    in.params.call_data.to = deployed_address;
    in.params.call_data.data = mul_100;
    do_rpc(frontend, cert, in); // CHECKs internally that RPC doesn't error
  }
}

TEST_CASE("SendTransaction1" * doctest::test_suite("transactions"))
{
  // Deploys a contract that uses storage
  // See SimpleStore.sol for source
  NetworkTables nwt;
  StubNotifier stubn;
  Store& tables = *nwt.tables;
  auto cert = setup_tables(tables);
  Ethereum frontend = ccfapp::get_rpc_handler(nwt, stubn);

  const auto store_compiled = read_bytecode("SimpleStore");

  const auto constructor = abi_append(store_compiled.deploy, 15);

  const auto runtime = store_compiled.runtime;

  auto get = store_compiled.hashes["get()"];
  auto add2 = abi_append(store_compiled.hashes["add(uint256)"], 2);
  auto set1 = abi_append(store_compiled.hashes["set(uint256)"], 1);

  TestAccount owner(frontend, tables);

  // Create a contract by transaction
  eevm::Address storetest = owner.deploy_contract(constructor);

  // Check the contract is correctly deployed
  REQUIRE(owner.get_code(storetest) == runtime);
  CHECK(15 == get_result_value(owner.contract_call(storetest, get)));

  owner.contract_transact(storetest, add2);
  CHECK(17 == get_result_value(owner.contract_call(storetest, get)));
  owner.contract_transact(storetest, add2);
  CHECK(19 == get_result_value(owner.contract_call(storetest, get)));

  owner.contract_transact(storetest, set1);
  CHECK(1 == get_result_value(owner.contract_call(storetest, get)));
  owner.contract_transact(storetest, add2);
  CHECK(3 == get_result_value(owner.contract_call(storetest, get)));

  owner.contract_transact(storetest, set1);
  CHECK(1 == get_result_value(owner.contract_call(storetest, get)));
  owner.contract_transact(storetest, add2);
  owner.contract_transact(storetest, add2);
  owner.contract_transact(storetest, add2);
  owner.contract_transact(storetest, add2);
  CHECK(9 == get_result_value(owner.contract_call(storetest, get)));
}

TEST_CASE("SendTransaction2" * doctest::test_suite("transactions"))
{
  // Deploys a ballot contract.
  // See Ballot.sol for source.
  // Runs some plausible transactions to interact with it.
  // This tests a contract with persistent state, interaction with storage,
  // and some in-contract access controls.
  NetworkTables nwt;
  StubNotifier stubn;
  Store& tables = *nwt.tables;
  auto cert = setup_tables(tables);
  Ethereum frontend = ccfapp::get_rpc_handler(nwt, stubn);

  constexpr auto num_proposals = 5;
  const auto ballot_compiled = read_bytecode("Ballot");
  auto make_ballot_constructor = [&ballot_compiled](size_t n) {
    std::stringstream ss;
    ss << ballot_compiled.deploy << std::hex << std::setfill('0')
       << std::setw(64) << n;
    return ss.str();
  };
  const auto ballot_constructor = make_ballot_constructor(num_proposals);
  const auto ballot_deployed = ballot_compiled.runtime;
  auto winning_proposal = ballot_compiled.hashes["winningProposal()"];

  std::vector<std::string> vote;
  for (size_t i = 0; i < num_proposals; ++i)
  {
    vote.push_back(abi_append(ballot_compiled.hashes["vote(uint8)"], i));
  }

  TestAccount chairperson(frontend, tables);

  constexpr auto num_users = 4;
  std::vector<TestAccount> users;
  for (size_t i = 0; i < num_users; ++i)
    users.push_back(TestAccount(frontend, tables));

  auto make_give_right_to_vote = [&](const TestAccount& ta) {
    return abi_append(
      ballot_compiled.hashes["giveRightToVote(address)"], ta.address);
  };
  std::vector<std::string> give_right_to_vote;
  for (size_t i = 0; i < num_users; ++i)
  {
    give_right_to_vote.push_back(make_give_right_to_vote(users[i]));
  }

  std::vector<std::string> delegate;
  for (size_t i = 0; i < num_users; ++i)
  {
    delegate.push_back(abi_append(
      ballot_compiled.hashes["delegate(address)"], users[i].address));
  }

  // Create a contract by transaction
  eevm::Address ballot = chairperson.deploy_contract(ballot_constructor);
  REQUIRE(chairperson.get_code(ballot) == ballot_deployed);

  SUBCASE("default winner is 0")
  {
    const auto winner_hex = chairperson.contract_call(ballot, winning_proposal);
    const auto winner_n = get_result_value(winner_hex);
    CHECK(0 == winner_n);

    // Anyone can see this
    CHECK(
      0 == get_result_value(users[0].contract_call(ballot, winning_proposal)));
    CHECK(
      0 == get_result_value(users[1].contract_call(ballot, winning_proposal)));

    // Even new accounts
    TestAccount user_n(frontend, tables);
    CHECK(
      0 == get_result_value(user_n.contract_call(ballot, winning_proposal)));
  }

  SUBCASE("only chairperson can vote initially")
  {
    constexpr auto winner = 1;
    users[0].contract_transact(ballot, vote[winner]);
    users[1].contract_transact(ballot, vote[winner]);
    CHECK(
      0 ==
      get_result_value(chairperson.contract_call(ballot, winning_proposal)));

    TestAccount user_n(frontend, tables);
    user_n.contract_transact(ballot, vote[winner]);
    CHECK(
      0 ==
      get_result_value(chairperson.contract_call(ballot, winning_proposal)));

    chairperson.contract_transact(ballot, vote[winner]);
    CHECK(
      winner ==
      get_result_value(chairperson.contract_call(ballot, winning_proposal)));
    CHECK(
      winner ==
      get_result_value(users[0].contract_call(ballot, winning_proposal)));
    CHECK(
      winner ==
      get_result_value(user_n.contract_call(ballot, winning_proposal)));
  }

  SUBCASE("chairperson can give right to vote")
  {
    chairperson.contract_transact(ballot, give_right_to_vote[0]);
    chairperson.contract_transact(ballot, give_right_to_vote[1]);
    chairperson.contract_transact(ballot, give_right_to_vote[2]);

    // Un-nominated person still can't vote
    users[3].contract_transact(ballot, vote[2]);
    CHECK(
      0 ==
      get_result_value(chairperson.contract_call(ballot, winning_proposal)));

    // the nominees can
    users[0].contract_transact(ballot, vote[2]);
    CHECK(
      2 ==
      get_result_value(chairperson.contract_call(ballot, winning_proposal)));

    // they can outvote each other
    users[1].contract_transact(ballot, vote[1]);
    users[2].contract_transact(ballot, vote[1]);
    CHECK(
      1 ==
      get_result_value(chairperson.contract_call(ballot, winning_proposal)));

    // they can't change their vote/vote twice
    users[0].contract_transact(ballot, vote[2]);
    users[1].contract_transact(ballot, vote[2]);
    users[2].contract_transact(ballot, vote[2]);
    CHECK(
      1 ==
      get_result_value(chairperson.contract_call(ballot, winning_proposal)));
  }

  SUBCASE("votes can be delegated")
  {
    chairperson.contract_transact(ballot, give_right_to_vote[0]);
    chairperson.contract_transact(ballot, give_right_to_vote[1]);
    chairperson.contract_transact(ballot, give_right_to_vote[2]);
    chairperson.contract_transact(ballot, give_right_to_vote[3]);

    // chair votes for prop 1
    chairperson.contract_transact(ballot, vote[1]);
    CHECK(
      1 ==
      get_result_value(chairperson.contract_call(ballot, winning_proposal)));

    // 0 delegates to 1, who casts both votes for prop 2
    users[0].contract_transact(ballot, delegate[1]);
    users[1].contract_transact(ballot, vote[2]);
    CHECK(
      2 ==
      get_result_value(chairperson.contract_call(ballot, winning_proposal)));

    // 1 tries to delegate to 2, but this is ignored as 1 has voted
    // 2 votes for prop 3, but this isn't enough to change the result
    users[1].contract_transact(ballot, delegate[2]);
    users[2].contract_transact(ballot, vote[3]);
    CHECK(
      2 ==
      get_result_value(chairperson.contract_call(ballot, winning_proposal)));

    // 3 and a new user delegate to 2
    // their votes count towards prop 3 votes to 3, which is the new winner
    users[3].contract_transact(ballot, delegate[2]);
    TestAccount user_n(frontend, tables);
    chairperson.contract_transact(ballot, make_give_right_to_vote(user_n));
    user_n.contract_transact(ballot, delegate[2]);
    CHECK(
      3 ==
      get_result_value(chairperson.contract_call(ballot, winning_proposal)));
  }
}

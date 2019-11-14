// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

namespace evm4ccf
{
  inline void from_json(const nlohmann::json& j, TxResult& txr)
  {
    const auto it = j.find("address");
    if (it != j.end() && !it->is_null())
    {
      txr.contract_address = eevm::to_uint256(*it);
    }
    else
    {
      txr.contract_address = std::nullopt;
    }
    txr.logs = j["logs"].get<decltype(TxResult::logs)>();
  }

  inline void to_json(nlohmann::json& j, const TxResult& txr)
  {
    if (txr.contract_address.has_value())
    {
      j["address"] = eevm::to_hex_string(*txr.contract_address);
    }
    else
    {
      j["address"] = nullptr;
    }
    j["logs"] = txr.logs;
  }
} // namespace evm4ccf

// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "crypto/eth_signature.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("secp256k1-sign-verify" * doctest::test_suite("crypto"))
{
  const unsigned char private_key[32] = {0xCC};
  const unsigned char msg_hash[32] = {0xBF};
  int recovery_id = 0; // 0 <= recovery_id <= 3

  EthSignature sig(private_key, msg_hash);

  const EthSignature::public_key_bytes_t& public_key =
    sig.recover_public_key(msg_hash);

  REQUIRE_NOTHROW(sig.verify(public_key, msg_hash));
}

TEST_CASE("secp256k1-sign-verify-recovered" * doctest::test_suite("crypto"))
{
  const unsigned char private_key[32] = {0xCC};
  const unsigned char msg_hash[32] = {0xBF};
  int recovery_id = 0; // 0 <= recovery_id <= 3

  EthSignature sig(private_key, msg_hash);

  const EthSignature::public_key_bytes_t& public_key =
    sig.recover_public_key(msg_hash);

  EthSignature sig2;
  REQUIRE_NOTHROW(
    sig2.verifyRecovered(sig.toBytes(&recovery_id), msg_hash, recovery_id));
}

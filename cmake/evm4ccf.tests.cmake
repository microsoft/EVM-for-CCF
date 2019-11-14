# Tests

if(BUILD_TESTS)
  set(TESTS_DIR ${CMAKE_CURRENT_LIST_DIR}/../tests)
  set(SAMPLES_DIR ${CMAKE_CURRENT_LIST_DIR}/../samples)

  # Standalone test of app frontend
  add_unit_test(app_test
    ${TESTS_DIR}/shared.cpp
    ${TESTS_DIR}/kv_conversions.cpp
    ${TESTS_DIR}/eth_call.cpp
    ${TESTS_DIR}/eth_sendTransaction.cpp
    ${TESTS_DIR}/signed_transactions.cpp
    ${TESTS_DIR}/event_logs.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/app/evm_for_ccf.cpp
    ${EVM_CPP_FILES}
  )
  target_include_directories(app_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${EVM_DIR}/include
  )
  target_link_libraries(app_test PRIVATE
    keccak_enclave
    secp256k1.host
    intx::intx
  )
  
  set(ENV_CONTRACTS_DIR "CONTRACTS_DIR=${TESTS_DIR}/contracts")

  # Make compiled contracts available to app_test
  set_tests_properties(
    app_test
    PROPERTIES
      ENVIRONMENT "${ENV_CONTRACTS_DIR}"
  )

  # Add an end-to-end test using CCF's generic Python scenario client
  add_e2e_test(
    NAME erc20_scenario
    PYTHON_SCRIPT ${CCF_DIR}/tests/e2e_scenarios.py
    ADDITIONAL_ARGS
      --scenario ${TESTS_DIR}/erc20_transfers_scenario.json
  )

  # Add an end-to-end test using a web3.py-based client
  add_e2e_test(
    NAME erc20_python
    PYTHON_SCRIPT ${SAMPLES_DIR}/erc20_deploy_and_transfer.py
  )

  # Make python test client framework importable for all end-to-end tests
  set_tests_properties(
    erc20_scenario
    erc20_python
    PROPERTIES
      ENVIRONMENT "${ENV_CONTRACTS_DIR};PYTHONPATH=${CCF_DIR}/tests:$ENV{PYTHONPATH}"
  )
endif()
